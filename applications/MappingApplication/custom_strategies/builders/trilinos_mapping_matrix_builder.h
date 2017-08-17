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
//

#if !defined(KRATOS_TRILINOS_MAPPING_MATRIX_BUILDER_H_INCLUDED)
#define KRATOS_TRILINOS_MAPPING_MATRIX_BUILDER_H_INCLUDED

// System includes
#include "mpi.h"

// External includes

// Project includes
#include "includes/define.h"
#include "solving_strategies/schemes/scheme.h"


namespace Kratos
{
///@addtogroup MappingApplication
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
template <class TSparseSpace,
          class TDenseSpace, // = DenseSpace<double>,
          >
class TrilinosMappingMatrixBuilder : public MappingMatrixBuilder<TSparseSpace, TDenseSpace>
{
  public:
    ///@name Type Definitions
    ///@{

    /// Pointer definition of TrilinosMappingMatrixBuilder
    KRATOS_CLASS_POINTER_DEFINITION(TrilinosMappingMatrixBuilder);

    ///@}
    ///@name Life Cycle
    ///@{

    /// Default constructor.
    TrilinosMappingMatrixBuilder() {}

    /// Destructor.
    virtual ~TrilinosMappingMatrixBuilder() {}

    ///@}
    ///@name Operators
    ///@{

    ///@}
    ///@name Operations
    ///@{

    void BuildLHS(ModelPart& rModelPart,
                          TSystemMatrixType& A) override
    {   

    }

    void BuildRHS(ModelPart& rModelPart,
                          TSystemVectorType& b) override
    {


    }

    void SetUpDofSet(ModelPart& r_model_part
    ) override
    {
    }

    /**
            organises the dofset in order to speed up the building phase
     */
    void SetUpSystem(
        ModelPart& r_model_part
    ) override
    {
    }

    void ResizeAndInitializeVectors(
        TSystemMatrixPointerType& pA,
        TSystemVectorPointerType& pDx,
        TSystemVectorPointerType& pb,
        ElementsArrayType& rElements,
        ConditionsArrayType& rConditions,
        ProcessInfo& CurrentProcessInfo
    ) override
    {
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
        buffer << "TrilinosMappingMatrixBuilder";
        return buffer.str();
    }

    /// Print information about this object.
    virtual void PrintInfo(std::ostream &rOStream) const { rOStream << "TrilinosMappingMatrixBuilder"; }

    /// Print object's data.
    virtual void PrintData(std::ostream &rOStream) const {}

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

    /**
    This functions computes the start equationId for to local nodes
    aka the equation id of the global system by asking the ranks
    with an rank-id smaller than my own rank id for their number
    of nodes
    Example:
    r0 : 5 Nodes (start EquationId: 0)
    r1 : 7 Nodes (start EquationId: 5)
    r2 : 8 Nodes (start EquationId: 12)
    */
    int GetStartEquationId(ModelPart& rModelPart) override
    {   
        const int num_local_nodes = rModelPart.GetCommunicator().LocalMesh().NumberOfNodes()

        const int comm_rank = rModelPart.GetCommunicator().MyPID();
        const int comm_size = rModelPart.GetCommunicator().TotalProcesses();

        int* num_nodes_list = new int[comm_size];

        // get the number of nodes on the other ranks
        MPI_Allgather(num_local_nodes, 1, MPI_INT, num_nodes_list,
                    1, MPI_INT, MPI_COMM_WORLD);

        int start_equation_id = 0;
        for (int i = 0; i < comm_rank ; ++i) // loop the ranks before me
        {
            start_equation_id += num_nodes_list[i];
        }
 
        delete num_nodes_list;

        return start_equation_id;
    }

    virtual void GlobalUpdateVector(TSystemVectorType& b) {
        // Do the trilinos assembling here (if necessary, since the vector exists already ...)
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
    TrilinosMappingMatrixBuilder &operator=(TrilinosMappingMatrixBuilder const &rOther) {}

    /// Copy constructor.
    TrilinosMappingMatrixBuilder(TrilinosMappingMatrixBuilder const &rOther) {}

    ///@}

}; // Class TrilinosMappingMatrixBuilder

///@}

///@name Type Definitions
///@{

///@}
///@name Input and output
///@{

/// input stream function
inline std::istream &operator>>(std::istream &rIStream,
                                TrilinosMappingMatrixBuilder &rThis) {}

/// output stream function
inline std::ostream &operator<<(std::ostream &rOStream,
                                const TrilinosMappingMatrixBuilder &rThis)
{
    rThis.PrintInfo(rOStream);
    rOStream << std::endl;
    rThis.PrintData(rOStream);

    return rOStream;
}
///@}

///@} addtogroup block

} // namespace Kratos.

#endif // KRATOS_TRILINOS_MAPPING_MATRIX_BUILDER_H_INCLUDED  defined