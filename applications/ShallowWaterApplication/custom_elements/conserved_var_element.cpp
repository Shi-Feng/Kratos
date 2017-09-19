/* 
============================================================================== 
KratosShallowWaterApplication 
A library based on: 
Kratos 
A General Purpose Software for Multi-Physics Finite Element Analysis 
Version 1.0 (Released on march 05, 2007). 
 
Copyright 2007 
Pooyan Dadvand, Riccardo Rossi 
pooyan@cimne.upc.edu 
rrossi@cimne.upc.edu 
- CIMNE (International Center for Numerical Methods in Engineering), 
Gran Capita' s/n, 08034 Barcelona, Spain 
 
 
Permission is hereby granted, free  of charge, to any person obtaining 
a  copy  of this  software  and  associated  documentation files  (the 
"Software"), to  deal in  the Software without  restriction, including 
without limitation  the rights to  use, copy, modify,  merge, publish, 
distribute,  sublicense and/or  sell copies  of the  Software,  and to 
permit persons to whom the Software  is furnished to do so, subject to 
the following condition: 
 
Distribution of this code for  any  commercial purpose  is permissible 
ONLY BY DIRECT ARRANGEMENT WITH THE COPYRIGHT OWNERS. 
 
The  above  copyright  notice  and  this permission  notice  shall  be 
included in all copies or substantial portions of the Software. 
 
THE  SOFTWARE IS  PROVIDED  "AS  IS", WITHOUT  WARRANTY  OF ANY  KIND, 
EXPRESS OR  IMPLIED, INCLUDING  BUT NOT LIMITED  TO THE  WARRANTIES OF 
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT  SHALL THE AUTHORS OR COPYRIGHT HOLDERS  BE LIABLE FOR ANY 
CLAIM, DAMAGES OR  OTHER LIABILITY, WHETHER IN AN  ACTION OF CONTRACT, 
TORT  OR OTHERWISE, ARISING  FROM, OUT  OF OR  IN CONNECTION  WITH THE 
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 
============================================================================== 
*/ 
// 
//   Project Name:        Kratos 
//   Last modified by:    Miguel Masó Sotomayor 
//   Date:                July 3rd 2017 
//   Revision:            1.2 
// 
// 
 
// Project includes 
#include "includes/define.h" 
#include "custom_elements/conserved_var_element.hpp" 
#include "shallow_water_application.h" 
#include "utilities/math_utils.h" 
#include "utilities/geometry_utilities.h" 
 
namespace Kratos 
{ 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::EquationIdVector(EquationIdVectorType& rResult, ProcessInfo& rCurrentProcessInfo) 
    { 
        KRATOS_TRY 
         
        unsigned int element_size = TNumNodes*3; 
        if(rResult.size() != element_size) 
            rResult.resize(element_size,false);                         // False says not to preserve existing storage!! 
 
        GeometryType& rGeom = GetGeometry(); 
        int counter=0; 
        for (unsigned int i = 0; i < TNumNodes; i++) 
        { 
            rResult[counter++] = rGeom[i].GetDof(MOMENTUM_X).EquationId(); 
            rResult[counter++] = rGeom[i].GetDof(MOMENTUM_Y).EquationId(); 
            rResult[counter++] = rGeom[i].GetDof(HEIGHT).EquationId(); 
        } 
         
        KRATOS_CATCH("") 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::GetDofList(DofsVectorType& rElementalDofList,ProcessInfo& rCurrentProcessInfo) 
    { 
        KRATOS_TRY 
         
        unsigned int element_size = TNumNodes*3; 
        if(rElementalDofList.size() != element_size) 
            rElementalDofList.resize(element_size); 
 
        GeometryType& rGeom = GetGeometry(); 
        int counter=0; 
        for (unsigned int i = 0; i < TNumNodes; i++) 
        { 
            rElementalDofList[counter++] = rGeom[i].pGetDof(MOMENTUM_X); 
            rElementalDofList[counter++] = rGeom[i].pGetDof(MOMENTUM_Y); 
            rElementalDofList[counter++] = rGeom[i].pGetDof(HEIGHT); 
        } 
         
        KRATOS_CATCH("") 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::CalculateLocalSystem(MatrixType& rLeftHandSideMatrix, VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) 
    { 
        KRATOS_TRY 
         
        // Resize of the Left and Right Hand side 
        unsigned int element_size = TNumNodes*3; 
        if(rLeftHandSideMatrix.size1() != element_size) 
            rLeftHandSideMatrix.resize(element_size,element_size,false); // Resizing the system in case it does not have the right size 
         
        if(rRightHandSideVector.size() != element_size) 
            rRightHandSideVector.resize(element_size,false); 
 
        // Getting gravity 
        //~ array_1d<double,3> v_gravity = rCurrentProcessInfo[GRAVITY]; 
        double gravity = 9.8; //-v_gravity[2]; 
     
        // Getting the time step (not fixed to allow variable time step) 
        const double delta_t = rCurrentProcessInfo[DELTA_TIME]; 
        const double dt_inv = 1.0 / delta_t; 
     
        // Compute the geometry 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes, 2> DN_DX; 
        array_1d<double,TNumNodes> N; 
        double Area; 
        this-> CalculateGeometry(DN_DX,Area); 
        double elem_length = this->ComputeElemSize(DN_DX); 
         
        // Getting the values of shape functions on Integration Points 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes, TNumNodes> Ncontainer;  // In this case, number of Gauss points and number of nodes coincides 
        const GeometryType& rGeom = this->GetGeometry(); 
        Ncontainer = rGeom.ShapeFunctionsValues( GeometryData::GI_GAUSS_2 ); 
     
        // Get nodal values for current step and projected variables 
        array_1d<double, TNumNodes*3> v_depth; 
        array_1d<double, TNumNodes*3> v_unknown; 
        array_1d<double, TNumNodes*3> v_proj_unknown; 
        double height; 
        double div_u; 
        GetNodalValues(v_depth,v_unknown,v_proj_unknown); 
        GetElementValues(DN_DX,v_unknown,height,div_u); 
     
        // Some auxilary definitions 
        boost::numeric::ublas::bounded_matrix<double,2,TNumNodes*3> N_mom        = ZeroMatrix(2,TNumNodes*3);  // Shape functions matrix (for momentum unknown) 
        boost::numeric::ublas::bounded_matrix<double,1,TNumNodes*3> N_height     = ZeroMatrix(1,TNumNodes*3);  // Shape functions vector (for height unknown) 
        boost::numeric::ublas::bounded_matrix<double,1,TNumNodes*3> DN_DX_mom    = ZeroMatrix(1,TNumNodes*3);  // Shape functions divergence vector (for momentum unknown) 
        boost::numeric::ublas::bounded_matrix<double,2,TNumNodes*3> DN_DX_height = ZeroMatrix(2,TNumNodes*3);  // Shape functions gradient matrix (for height unknown) 
        boost::numeric::ublas::bounded_matrix<double,2,TNumNodes*3> grad_mom     = ZeroMatrix(2,TNumNodes*3);  // Shaoe functions gradient vector (for momentum unknown) 
        // 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes*3,TNumNodes*3> mass_matrix  = ZeroMatrix(TNumNodes*3,TNumNodes*3); 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes*3,TNumNodes*3> aux_q_div_m  = ZeroMatrix(TNumNodes*3,TNumNodes*3); 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes*3,TNumNodes*3> aux_w_grad_h = ZeroMatrix(TNumNodes*3,TNumNodes*3); 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes*3,TNumNodes*3> aux_m_diffus = ZeroMatrix(TNumNodes*3,TNumNodes*3); 
        boost::numeric::ublas::bounded_matrix<double,TNumNodes*3,TNumNodes*3> aux_h_diffus = ZeroMatrix(TNumNodes*3,TNumNodes*3); 
 
        // Loop on Gauss points. In this case, number of Gauss points are equal to number of nodes 
        for (unsigned int igauss = 0; igauss < TNumNodes; igauss++)
        {
            noalias(N) = row(Ncontainer, igauss);
            
            // Build shape and derivatives functions at Gauss points
            for (unsigned int nnode = 0; nnode < TNumNodes; nnode++)
            {
                // Height gradient 
                DN_DX_height(0, 2+nnode*3) = DN_DX(nnode,0); 
                DN_DX_height(1, 2+nnode*3) = DN_DX(nnode,1); 
                // Momentum divergence 
                DN_DX_mom(0,   nnode*3) = DN_DX(nnode,0); 
                DN_DX_mom(0, 1+nnode*3) = DN_DX(nnode,1); 
                // Momentum gradient 
                grad_mom(0,   nnode*3) = DN_DX(nnode,0); 
                grad_mom(0, 1+nnode*3) = DN_DX(nnode,0); 
                grad_mom(1,   nnode*3) = DN_DX(nnode,1); 
                grad_mom(1, 1+nnode*3) = DN_DX(nnode,1); 
                // Height shape funtions 
                N_height(0, 2+nnode*3) = N[nnode]; 
                // Momentum shape functions 
                N_mom(0,   nnode*3) = N[nnode]; 
                N_mom(1, 1+nnode*3) = N[nnode]; 
            }
       
        noalias(mass_matrix)  += prod(trans(N_mom),N_mom); 
        noalias(mass_matrix)  += prod(trans(N_height),N_height); 
         
        noalias(aux_q_div_m)  += prod(trans(N_height),DN_DX_mom); 
        noalias(aux_w_grad_h) += prod(trans(N_mom),DN_DX_height); 
        
        noalias(aux_m_diffus) += prod(trans(DN_DX_mom),DN_DX_mom); 
        noalias(aux_h_diffus) += prod(trans(DN_DX_height),DN_DX_height); 
        }
        
        //~ CalculateLumpedMassMatrix(mass_matrix);
 
        // Copmute stabilization parameters 
        bool stabilization = true; 
        double height_threshold = 1e-6; 
        double Ctau = 0.01;       // Stabilization parameter >0.005 (R.Codina, CMAME 197, 2008, 1305-1322) 
        double tau_h = 0, tau_m = 0; 
        if (stabilization && height > height_threshold) 
        { 
            tau_m = Ctau/elem_length*pow(gravity/height,0.5); 
            tau_h = Ctau/elem_length*pow(height/gravity,0.5); 
        } 
        // Compute discontinuity capturing parameters 
        bool discontinuity_capturing = true; 
        double gradient_threshold = 1e-6;    // Shock capturing parameters 
        //~ double residual = norm_1(prod(msN_height,ms_unknown)) * norm_1(prod(msDN_DX_mom,ms_unknown)) + dt_inv*norm_1(prod(msN_height, (ms_unknown - ms_proj_unknown))); 
        double height_grad_norm = norm_2(prod(DN_DX_height,v_unknown)); 
        double k_dc = 0; 
        if (discontinuity_capturing && height_grad_norm > gradient_threshold) 
        { 
            // Residual formulation 
            k_dc = 0.5*0.4*elem_length*height_grad_norm; 
            // Print values on Gauss points 
            //~ this->SetValue(RESIDUAL_NORM,residual); 
            this->SetValue(MIU,height_grad_norm); 
            this->SetValue(PR_ART_VISC,k_dc); 
        }
 
        // Build LHS 
        // Cross terms 
        noalias(rLeftHandSideMatrix)  = gravity * height * aux_w_grad_h; // Add <w,g*h*grad(h)> to Momentum Eq. 
 
        // Inertia terms 
        noalias(rLeftHandSideMatrix) += dt_inv * mass_matrix;           // Add <N,N> to both Eq's 
     
        // Non linear terms 
        noalias(rLeftHandSideMatrix) += div_u * mass_matrix;             // Add <q,div(u)*h> to Mass Eq. and <w,div(u)*hu> to Momentum Eq. 
 
        // Stabilization terms 
        noalias(rLeftHandSideMatrix) += (k_dc + tau_h) * aux_h_diffus;  // Add art. diff. to Mass Eq. 
        noalias(rLeftHandSideMatrix) +=         tau_m  * aux_m_diffus;  // Add art. diff. to Momentum Eq. 
         
        // Build RHS 
        // Source terms (bathymetry contribution) 
        noalias(rRightHandSideVector)  = -gravity * prod(aux_w_grad_h, v_depth); // Add <w,-g*h*grad(H)> to RHS (Momentum Eq.) 
     
        // Inertia terms 
        noalias(rRightHandSideVector) += dt_inv * prod(mass_matrix, v_proj_unknown); 
     
        // Subtracting the dirichlet term (since we use a residualbased approach) 
        noalias(rRightHandSideVector) -= prod(rLeftHandSideMatrix, v_unknown); 
     
        rRightHandSideVector *= Area / static_cast<double>(TNumNodes); 
        rLeftHandSideMatrix *= Area  / static_cast<double>(TNumNodes); 
     
        KRATOS_CATCH(""); 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::CalculateRightHandSide(VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) 
    { 
        KRATOS_THROW_ERROR(std::logic_error,  "method not implemented" , ""); 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::GetValueOnIntegrationPoints(const Variable<double>& rVariable, std::vector<double>& rValues, const ProcessInfo& rCurrentProcessInfo) 
    { 
        if (rVariable == VEL_ART_VISC || rVariable == PR_ART_VISC || rVariable == RESIDUAL_NORM || rVariable == MIU) 
        { 
            for (unsigned int PointNumber = 0; PointNumber < 1; PointNumber++) 
                rValues[PointNumber] = double(this->GetValue(rVariable)); 
        } 
    }
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::CalculateGeometry(boost::numeric::ublas::bounded_matrix<double, TNumNodes, 2>& rDN_DX, double& rArea) 
    { 
        const GeometryType& rGeom = this->GetGeometry(); 
 
        // We select GI_GAUSS_1 due to we are computing at the barycenter. 
        const GeometryType::IntegrationPointsArrayType& integration_points = rGeom.IntegrationPoints(GeometryData::GI_GAUSS_1); 
        const unsigned int NumGPoints = integration_points.size(); 
        rArea = rGeom.Area(); 
        GeometryType::ShapeFunctionsGradientsType DN_DXContainer( NumGPoints ); 
        rGeom.ShapeFunctionsIntegrationPointsGradients(DN_DXContainer, GeometryData::GI_GAUSS_1); 
 
        noalias( rDN_DX ) = DN_DXContainer[0]; 
 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    double ConservedVarElement<TNumNodes>::ComputeElemSize(boost::numeric::ublas::bounded_matrix<double, TNumNodes, 2>& rDN_DX) 
    { 
        double l = 0.0; 
 
        for(unsigned int i = 0; i < TNumNodes; i++) 
        { 
            double l_inv = 0.0; 
            for(unsigned int k = 0; k < 2; k++) 
            { 
                l_inv += rDN_DX(i,k) * rDN_DX(i,k); 
            } 
            l += 1.0 / l_inv; 
        } 
        l = sqrt(l) / static_cast<double>(TNumNodes); 
        return l; 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::GetNodalValues(array_1d<double, TNumNodes*3>& rdepth, 
                                                        array_1d<double, TNumNodes*3>& runkn,  
                                                        array_1d<double, TNumNodes*3>& rproj 
                                                       ) 
    { 
        GeometryType& rGeom = GetGeometry(); 
        unsigned int counter = 0; 
        for (unsigned int i = 0; i < TNumNodes; i++) 
        { 
            rdepth[counter] = 0; 
            runkn[counter]  = rGeom[i].FastGetSolutionStepValue(MOMENTUM_X); 
            rproj[counter]  = rGeom[i].FastGetSolutionStepValue(PROJECTED_MOMENTUM_X); 
            counter++; 
 
            rdepth[counter] = 0; 
            runkn[counter]  = rGeom[i].FastGetSolutionStepValue(MOMENTUM_Y); 
            rproj[counter]  = rGeom[i].FastGetSolutionStepValue(PROJECTED_MOMENTUM_Y); 
            counter++; 
 
            rdepth[counter] = rGeom[i].FastGetSolutionStepValue(BATHYMETRY); 
            runkn[counter]  = rGeom[i].FastGetSolutionStepValue(HEIGHT); 
            rproj[counter]  = rGeom[i].FastGetSolutionStepValue(PROJECTED_HEIGHT); 
            counter++; 
        } 
    } 
 
//---------------------------------------------------------------------- 
 
    template< unsigned int TNumNodes > 
    void ConservedVarElement<TNumNodes>::GetElementValues(boost::numeric::ublas::bounded_matrix<double,TNumNodes, 2>& rDN_DX, 
                                                          array_1d<double, TNumNodes*3>& r_nodal_var, 
                                                          double& rheight, 
                                                          double& rdiv_u 
                                                         ) 
    {
        double lumping_factor = 1 / double(TNumNodes); 
         
        rheight = 0; 
        rdiv_u  = 0; 
         
        for (unsigned int i = 0; i < TNumNodes; i++) 
        { 
            rheight += r_nodal_var[2 + 3*i]; 
            rdiv_u  += rDN_DX(i,0) * r_nodal_var[  + 3*i] / r_nodal_var[2 + 3*i]; 
            rdiv_u  += rDN_DX(i,1) * r_nodal_var[1 + 3*i] / r_nodal_var[2 + 3*i]; 
        } 
 
        rheight *= lumping_factor; 
    }

//----------------------------------------------------------------------

    template< unsigned int TNumNodes >
    void ConservedVarElement<TNumNodes>::CalculateLumpedMassMatrix(boost::numeric::ublas::bounded_matrix<double, TNumNodes*3, TNumNodes*3>& rMassMatrix) 
    {
        const unsigned int element_size = 3*TNumNodes;
        rMassMatrix  = IdentityMatrix(element_size, element_size);
        rMassMatrix /= static_cast<double>(TNumNodes);
    }
 
//---------------------------------------------------------------------- 
 
template class ConservedVarElement<3>; 
template class ConservedVarElement<4>; 
 
} // namespace Kratos 