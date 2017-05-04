//
// ==============================================================================
//  ChimeraApplication
//
//  License:         BSD License
//                   license: ChimeraApplication/license.txt
//
//  Main authors:    Aditya Ghantasala, https://github.com/adityaghantasala
//                   Navaneeth K Narayanan
//
// ==============================================================================

// System includes


// External includes
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/timer.hpp>


// Project includes
#include "includes/define.h"
#include "custom_python/add_custom_strategies_to_python.h"


#include "spaces/ublas_space.h"

// Strategies
#include "solving_strategies/strategies/solving_strategy.h"
//#include "custom_strategies/custom_strategies/residual_based_arc_length_strategy.hpp"
//#include "custom_strategies/custom_strategies/eigensolver_strategy.hpp"

// Schemes
#include "solving_strategies/schemes/scheme.h"
//#include "custom_strategies/custom_schemes/residual_based_relaxation_scheme.hpp"
//#include "custom_strategies/custom_schemes/eigensolver_dynamic_scheme.hpp"

// Builder and solvers
#include "solving_strategies/builder_and_solvers/residualbased_block_builder_and_solver.h"
#include "custom_strategies/custom_builder_and_solver/residualbased_block_builder_and_solver_with_mpc_chimera.h"

// Convergence criterias
#include "solving_strategies/convergencecriterias/convergence_criteria.h"
#include "linear_solvers/linear_solver.h"


namespace Kratos
{

namespace Python
{
using namespace boost::python;

void  AddCustomStrategiesToPython()
{
    typedef UblasSpace<double, CompressedMatrix, Vector> SparseSpaceType;
    typedef UblasSpace<double, Matrix, Vector> LocalSpaceType;
  
    // Base types
    typedef LinearSolver<SparseSpaceType, LocalSpaceType > LinearSolverType;
    
    
    
    // Custom convergence criterion types

    // Custom builder and solvers types
    
    //********************************************************************
    //*************************STRATEGY CLASSES***************************
    //********************************************************************
    
    

    //********************************************************************
    //*******************CONVERGENCE CRITERIA CLASSES*********************
    //********************************************************************
            
    //********************************************************************
    //*************************BUILDER AND SOLVER*************************
    //********************************************************************
    class_< ResidualBasedBlockBuilderAndSolverWithMpcChimera< SparseSpaceType, LocalSpaceType, LinearSolverType >,
                bases< ResidualBasedBlockBuilderAndSolver< SparseSpaceType, LocalSpaceType, LinearSolverType > >,
                boost::noncopyable >
                ("ResidualBasedBlockBuilderAndSolverWithMpcChimera", init<LinearSolverType::Pointer>());
}

}  // namespace Python.

} // Namespace Kratos

