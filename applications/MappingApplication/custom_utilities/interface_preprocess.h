//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:		 BSD License
//					 Kratos default license: kratos/license.txt
//
//  Main authors:    Philipp Bucher, Jordi Cotela
//
// See Master-Thesis P.Bucher
// "Development and Implementation of a Parallel
//  Framework for Non-Matching Grid Mapping"

#if !defined(KRATOS_INTERFACE_PREPROCESS_H_INCLUDED )
#define  KRATOS_INTERFACE_PREPROCESS_H_INCLUDED



// System includes
#include <string>
#include <iostream> 


// External includes 


// Project includes
#include "includes/model_part.h"
#include "includes/define.h"
#include "includes/kratos_parameters.h"

#ifdef KRATOS_USING_MPI // mpi-parallel compilation
#include "custom_utilities/parallel_fill_communicator.h"
#endif


namespace Kratos
{
  ///@addtogroup ApplicationNameApplication
  ///@{

  ///@name Kratos Globals
  ///@{ 
  
  ///@} 
  ///@name Type Definitions
  ///@{ 
  
  ///@} 
  ///@name  Enum's
  ///@{
      
  ///@}
  ///@name  Functions 
  ///@{
      
  ///@}
  ///@name Kratos Classes
  ///@{
  
  /// Short class definition.
  /** Detail class definition.
  */
  class InterfacePreprocess
    {
    public:
        ///@name Type Definitions
        ///@{
        
        /// Pointer definition of InterfacePreprocess
        KRATOS_CLASS_POINTER_DEFINITION(InterfacePreprocess);
    
        ///@}
        ///@name Life Cycle 
        ///@{ 
        
        /// Default constructor.
        InterfacePreprocess(ModelPart& rModelPart, ModelPart::Pointer pInterfaceModelPart) : 
            mrModelPart(rModelPart), 
            mpInterfaceModelPart(pInterfaceModelPart) {}

        /// Destructor.
        virtual ~InterfacePreprocess(){}
        

        ///@}
        ///@name Operators 
        ///@{
        
        
        ///@}
        ///@name Operations
        ///@{

        void GenerateInterfacePart(Parameters InterfaceParameters)
        {
            CheckAndValidateParameters(InterfaceParameters);      

            // Create a new / overwrite the InterfaceModelpart
            KRATOS_WATCH(mpInterfaceModelPart)
            mpInterfaceModelPart = ModelPart::Pointer( new ModelPart("Mapper-Interface") ); // TODO why is this not working?
            KRATOS_WATCH(*mpInterfaceModelPart)
            
            AddNodes();
            CreateConditions(InterfaceParameters);

#ifdef KRATOS_USING_MPI // mpi-parallel compilation
            if (MapperUtilities::TotalProcesses() > 1)
            {
                // Set the MPICommunicator
                std::cout << "Doing the ParallelFillCommunicator stuff" << std::endl;
                ParallelFillCommunicator parallel_fill_communicator(*mpInterfaceModelPart);
                parallel_fill_communicator.Execute();
            }
#endif
            
        }
        
        
        ///@}
        ///@name Access
        ///@{ 
        
        
        ///@}
        ///@name Inquiry
        ///@{
        
        
        ///@}      
        ///@name Input and output
        ///@{

        /// Turn back information as a string.
        virtual std::string Info() const
        {
            std::stringstream buffer;
            buffer << "InterfacePreprocess" ;
            return buffer.str();
        }
        
        /// Print information about this object.
        virtual void PrintInfo(std::ostream& rOStream) const {rOStream << "InterfacePreprocess";}

        /// Print object's data.
        virtual void PrintData(std::ostream& rOStream) const {}
        
                
        ///@}      
        ///@name Friends
        ///@{
        
                
        ///@}
      
    protected:
        ///@name Protected static Member Variables 
        ///@{ 
            
            
        ///@} 
        ///@name Protected member Variables 
        ///@{ 
            
            
        ///@} 
        ///@name Protected Operators
        ///@{ 
            
            
        ///@} 
        ///@name Protected Operations
        ///@{ 
            
            
        ///@} 
        ///@name Protected  Access 
        ///@{ 
            
            
        ///@}      
        ///@name Protected Inquiry 
        ///@{ 
            
            
        ///@}    
        ///@name Protected LifeCycle 
        ///@{ 
        
                
        ///@}
      
    private:
        ///@name Static Member Variables 
        ///@{ 
            
            
        ///@} 
        ///@name Member Variables 
        ///@{ 
        ModelPart& mrModelPart;
        ModelPart::Pointer mpInterfaceModelPart;

        Parameters mDefaultParameters = Parameters( R"(
        {
            "condition_name" : "",
            "use_nodes"      : false
        }  )" );
            
            
        ///@} 
        ///@name Private Operators
        ///@{ 
            
            
        ///@} 
        ///@name Private Operations
        ///@{ 
            
        void CheckAndValidateParameters(Parameters InterfaceParameters)
        {
            InterfaceParameters.RecursivelyValidateAndAssignDefaults(mDefaultParameters);
            std::string condition_name = InterfaceParameters["condition_name"].GetString();
            
            KRATOS_ERROR_IF(condition_name == "") << "Condition name for Interface-ModelPart not specified" << std::endl;
        }

        void AddNodes()
        {
            KRATOS_WATCH("Reached the node fct")
            // Store pointers to all interface nodes
            unsigned int nodes_counter = 0;
            for (ModelPart::NodesContainerType::const_iterator node_it = mrModelPart.NodesBegin(); node_it != mrModelPart.NodesEnd(); ++node_it)
            {
                mpInterfaceModelPart->Nodes().push_back( *(node_it.base()) ); // TODO resize? I know the size in advance...
                ++nodes_counter;
            }
        }

        void CreateConditions(Parameters InterfaceParameters)
        {

        }
            
        ///@} 
        ///@name Private  Access 
        ///@{ 
            
            
        ///@}    
        ///@name Private Inquiry 
        ///@{ 
            
            
        ///@}    
        ///@name Un accessible methods 
        ///@{ 
        
        /// Assignment operator.
        //   InterfacePreprocess& operator=(InterfacePreprocess const& rOther){}

        /// Copy constructor.
        // InterfacePreprocess(InterfacePreprocess const& rOther){}

            
        ///@}    
        
    }; // Class InterfacePreprocess 

  ///@} 
  
  ///@name Type Definitions       
  ///@{ 
  
  
  ///@} 
  ///@name Input and output 
  ///@{ 
        
 
  /// input stream function
//   inline std::istream& operator >> (std::istream& rIStream, 
// 				    InterfacePreprocess& rThis){}

  /// output stream function
  inline std::ostream& operator << (std::ostream& rOStream, 
				    const InterfacePreprocess& rThis)
    {
      rThis.PrintInfo(rOStream);
      rOStream << std::endl;
      rThis.PrintData(rOStream);

      return rOStream;
    }
  ///@}

  ///@} addtogroup block
  
}  // namespace Kratos.

#endif // KRATOS_INTERFACE_PREPROCESS_H_INCLUDED  defined 

