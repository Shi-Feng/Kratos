//
//   Project Name:        KratosConstitutiveModelsApplication $
//   Created by:          $Author:                JMCarbonell $
//   Last modified by:    $Co-Author:                         $
//   Date:                $Date:                   April 2017 $
//   Revision:            $Revision:                      0.0 $
//
//

#if !defined(KRATOS_SIMO_JU_EXPONENTIAL_DAMAGE_MODEL_H_INCLUDED )
#define  KRATOS_SIMO_JU_EXPONENTIAL_DAMAGE_MODEL_H_INCLUDED

// System includes

// External includes

// Project includes
#include "custom_models/plasticity_models/damage_model.hpp"
#include "custom_models/plasticity_models/yield_surfaces/simo_ju_yield_surface.hpp"
#include "custom_models/plasticity_models/hardening_rules/exponential_damage_hardening_rule.hpp"
#include "custom_models/elasticity_models/linear_elastic_model.hpp"

namespace Kratos
{
  ///@addtogroup ConstitutiveModelsApplication
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
  class KRATOS_API(CONSTITUTIVE_MODELS_APPLICATION) SimoJuExponentialDamageModel : public DamageModel<LinearElasticModel, SimoJuYieldSurface<ExponentialDamageHardeningRule> >
  {
  public:
    
    ///@name Type Definitions
    ///@{

    //elasticity model
    typedef LinearElasticModel                             ElasticityModelType;
    typedef typename ElasticityModelType::Pointer       ElasticityModelPointer;

    //yield surface
    typedef ExponentialDamageHardeningRule                   HardeningRuleType;
    typedef SimoJuYieldSurface<HardeningRuleType>             YieldSurfaceType;
    typedef typename YieldSurfaceType::Pointer             YieldSurfacePointer;

    //base type
    typedef DamageModel<ElasticityModelType,YieldSurfaceType>         BaseType;

    //common types
    typedef typename BaseType::Pointer                         BaseTypePointer;
    typedef typename BaseType::SizeType                               SizeType;
    typedef typename BaseType::VoigtIndexType                   VoigtIndexType;
    typedef typename BaseType::MatrixType                           MatrixType;
    typedef typename BaseType::ModelDataType                     ModelDataType;
    typedef typename BaseType::MaterialDataType               MaterialDataType;
    typedef typename BaseType::PlasticDataType                 PlasticDataType;
    typedef typename BaseType::InternalVariablesType     InternalVariablesType;


    /// Pointer definition of SimoJuExponentialDamageModel
    KRATOS_CLASS_POINTER_DEFINITION( SimoJuExponentialDamageModel );

    ///@}
    ///@name Life Cycle
    ///@{

    /// Default constructor.
    SimoJuExponentialDamageModel() : BaseType() {}
    
    /// Copy constructor.
    SimoJuExponentialDamageModel(SimoJuExponentialDamageModel const& rOther)
      :BaseType(rOther) {}

    /// Assignment operator.
    SimoJuExponentialDamageModel& operator=(SimoJuExponentialDamageModel const& rOther)
    {
      BaseType::operator=(rOther);
      return *this;
    }

    /// Clone.
    ConstitutiveModel::Pointer Clone() const override
    {
      return ( SimoJuExponentialDamageModel::Pointer(new SimoJuExponentialDamageModel(*this)) );
    }
    
    /// Destructor.
    virtual ~SimoJuExponentialDamageModel() {}


    ///@}
    ///@name Operators
    ///@{


    ///@}
    ///@name Operations
    ///@{


    ///@}
    ///@name Access
    ///@{

    /**
     * Has Values
     */   
    virtual bool Has(const Variable<double>& rThisVariable) override
    {
      return false;
    }
    

    /**
     * Get Values
     */
    virtual double& GetValue(const Variable<double>& rThisVariable, double& rValue) override
    {
      
      rValue=0;
           
      return rValue;
    }
    
    ///@}
    ///@name Inquiry
    ///@{


    ///@}
    ///@name Input and output
    ///@{

    /// Turn back information as a string.
    virtual std::string Info() const override
    {
      std::stringstream buffer;
      buffer << "SimoJuExponentialDamageModel" ;
      return buffer.str();
    }

    /// Print information about this object.
    virtual void PrintInfo(std::ostream& rOStream) const override
    {
      rOStream << "SimoJuExponentialDamageModel";
    }

    /// Print object's data.
    virtual void PrintData(std::ostream& rOStream) const override
    {
      rOStream << "SimoJuExponentialDamageModel Data";
    }

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

	
    ///@}
    ///@name Private Operators
    ///@{


    ///@}
    ///@name Private Operations
    ///@{


    ///@}
    ///@name Private  Access
    ///@{


    ///@}
    ///@name Private Inquiry
    ///@{


    ///@}
    ///@name Serialization
    ///@{    
    friend class Serializer;

    virtual void save(Serializer& rSerializer) const override
    {
      KRATOS_SERIALIZE_SAVE_BASE_CLASS( rSerializer, BaseType )
    }
    
    virtual void load(Serializer& rSerializer) override
    {
      KRATOS_SERIALIZE_LOAD_BASE_CLASS( rSerializer, BaseType )
    }

    ///@}
    ///@name Un accessible methods
    ///@{


    ///@}

  }; // Class SimoJuExponentialDamageModel

  ///@}

  ///@name Type Definitions
  ///@{


  ///@}
  ///@name Input and output
  ///@{

  
  ///@} 
  ///@name Input and output 
  ///@{

  
  ///@}

  ///@} addtogroup block


}  // namespace Kratos.

#endif // KRATOS_SIMO_JU_EXPONENTIAL_DAMAGE_MODEL_H_INCLUDED  defined 


