//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:         BSD License
//                   Kratos default license: kratos/license.txt
//
//  Main authors:    Miguel Masó Sotomayor
//


#if !defined(KRATOS_SHALLOW_WAER_APPLICATION_VARIABLES_H_INCLUDED )
#define  KRATOS_SHALLOW_WAER_APPLICATION_VARIABLES_H_INCLUDED

// System includes

// External includes

// Project includes
#include "includes/define.h"
#include "includes/kratos_application.h"
#include "includes/variables.h"

namespace Kratos
{
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, BATHYMETRY)

KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, HEIGHT)
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, PROJECTED_HEIGHT)
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, DELTA_HEIGHT)
KRATOS_DEFINE_3D_APPLICATION_VARIABLE_WITH_COMPONENTS( SHALLOW_WATER_APPLICATION, PROJECTED_VELOCITY )
KRATOS_DEFINE_3D_APPLICATION_VARIABLE_WITH_COMPONENTS( SHALLOW_WATER_APPLICATION, DELTA_VELOCITY )

KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, MEAN_SIZE)
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, MEAN_VEL_OVER_ELEM_SIZE)

KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, SCALAR_VELOCITY_X)
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, SCALAR_VELOCITY_Y)
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, SCALAR_PROJECTED_VELOCITY_X)
KRATOS_DEFINE_APPLICATION_VARIABLE( SHALLOW_WATER_APPLICATION, double, SCALAR_PROJECTED_VELOCITY_Y)
}

#endif	/* KRATOS_SHALLOW_WAER_APPLICATION_VARIABLES_H_INCLUDED */

