// KRATOS  ___|  |                   |                   |
//       \___ \  __|  __| |   |  __| __| |   |  __| _` | |
//             | |   |    |   | (    |   |   | |   (   | |
//       _____/ \__|_|   \__,_|\___|\__|\__,_|_|  \__,_|_| MECHANICS
//
//  License:		 BSD License
//					 license: StructuralMechanicsApplication/license.txt
//
//  Main authors:    Vicente Mataix Ferrandiz
//

// System includes

// External includes

// Project includes
#include "utilities/mortar_utilities.h"
#include "utilities/variable_utils.h"

/* Custom utilities */
#include "custom_utilities/contact_utilities.h"
#include "custom_utilities/tree_contact_search.h"

namespace Kratos
{
template<std::size_t TDim, std::size_t TNumNodes>
TreeContactSearch<TDim, TNumNodes>::TreeContactSearch(
    ModelPart & rMainModelPart,
    Parameters ThisParameters
    ):mrMainModelPart(rMainModelPart),
      mThisParameters(ThisParameters)
{
    KRATOS_ERROR_IF(mrMainModelPart.HasSubModelPart("Contact") == false) << "WARNING:: Please add the Contact submodelpart to your modelpart list" << std::endl;

    Parameters DefaultParameters = Parameters(R"(
    {
        "allocation_size"                      : 1000,
        "bucket_size"                          : 4,
        "search_factor"                        : 3.5,
        "type_search"                          : "InRadius",
        "check_gap"                            : "MappingCheck",
        "condition_name"                       : "",
        "final_string"                         : "",
        "inverted_search"                      : false,
        "dynamic_search"                       : false,
        "double_formulation"                   : false,
        "predefined_master_slave"              : true
    })" );

    mThisParameters.ValidateAndAssignDefaults(DefaultParameters);

    mCheckGap = ConvertCheckGap(mThisParameters["check_gap"].GetString());
    mInvertedSearch = mThisParameters["inverted_search"].GetBool();
    mPredefinedMasterSlave = mThisParameters["predefined_master_slave"].GetBool();

    // Check if the computing contact submodelpart
    if (mrMainModelPart.HasSubModelPart("ComputingContact") == false) // We check if the submodelpart where the actual conditions used to compute contact are going to be computed
        mrMainModelPart.CreateSubModelPart("ComputingContact");
    else { // We clean the existing modelpart
        ModelPart& computing_rcontact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
        ConditionsArrayType& conditions_array = computing_rcontact_model_part.Conditions();
        const int num_conditions = static_cast<int>(conditions_array.size());

        #pragma omp parallel for
        for(int i = 0; i < num_conditions; ++i)
            (conditions_array.begin() + i)->Set(TO_ERASE, true);

        mrMainModelPart.RemoveConditionsFromAllLevels(TO_ERASE);
    }

    // Updating the base condition
    mConditionName = mThisParameters["condition_name"].GetString();
    if (mConditionName == "")
        mCreateAuxiliarConditions = false;
    else {
        mCreateAuxiliarConditions = true;
        std::ostringstream condition_name;
        condition_name << mConditionName << "Condition" << TDim << "D" << TNumNodes << "N" << mThisParameters["final_string"].GetString();
        mConditionName = condition_name.str();
    }

    // We get the contact model part
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");

    // We set to zero the NORMAL_GAP
    if (mCheckGap == CheckGap::MappingCheck)
        VariableUtils().SetNonHistoricalScalarVar<Variable<double>>(NORMAL_GAP, 0.0, rcontact_model_part.Nodes());

    // Iterate in the conditions
    ConditionsArrayType& conditions_array = rcontact_model_part.Conditions();
    const int num_conditions = static_cast<int>(conditions_array.size());

    #pragma omp parallel for
    for(int i = 0; i < num_conditions; ++i)
        (conditions_array.begin() + i)->Set(ACTIVE, false);

    // We identify the type of solution
    mTypeSolution =  TypeSolution::VectorLagrangeMultiplier;
    if (mrMainModelPart.NodesBegin()->SolutionStepsDataHas(VECTOR_LAGRANGE_MULTIPLIER) == false) {
        if (mrMainModelPart.NodesBegin()->SolutionStepsDataHas(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE))
            mTypeSolution = TypeSolution::NormalContactStress;
        else
            mTypeSolution = TypeSolution::ScalarLagrangeMultiplier;
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::InitializeMortarConditions()
{
    // Iterate in the conditions
    ConditionsArrayType& conditions_array = mrMainModelPart.GetSubModelPart("Contact").Conditions();
    const int num_conditions = static_cast<int>(conditions_array.size());

    #pragma omp parallel for
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = conditions_array.begin() + i;

        if (it_cond->Has(INDEX_SET) == false) it_cond->SetValue(INDEX_SET, Kratos::make_shared<IndexSet>());
//             it_cond->GetValue(INDEX_SET)->reserve(mThisParameters["allocation_size"].GetInt());
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::SetOriginDestinationModelParts(ModelPart& rModelPart)
{
    // We check if the MasterSubModelPart already exists
    if (rModelPart.HasSubModelPart("MasterSubModelPart") == false) {
        rModelPart.CreateSubModelPart("MasterSubModelPart");
    } else {
        rModelPart.RemoveSubModelPart("MasterSubModelPart");
        rModelPart.CreateSubModelPart("MasterSubModelPart");
    }
    // We check if the SlaveSubModelPart already exists
    if (rModelPart.HasSubModelPart("SlaveSubModelPart") == false) {
        rModelPart.CreateSubModelPart("SlaveSubModelPart");
    } else {
        rModelPart.RemoveSubModelPart("SlaveSubModelPart");
        rModelPart.CreateSubModelPart("SlaveSubModelPart");
    }

    ModelPart& r_master_model_part = rModelPart.GetSubModelPart("MasterSubModelPart");
    ModelPart& r_slave_model_part = rModelPart.GetSubModelPart("SlaveSubModelPart");

    // The vectors containing the ids
    std::vector<IndexType> slave_nodes_ids;
    std::vector<IndexType> master_nodes_ids;
    std::vector<IndexType> slave_conditions_ids;
    std::vector<IndexType> master_conditions_ids;

    #pragma omp parallel
    {
        // Creating a buffer for parallel vector fill
        std::vector<IndexType> slave_nodes_ids_buffer;
        std::vector<IndexType> master_nodes_ids_buffer;
        std::vector<IndexType> slave_conditions_ids_buffer;
        std::vector<IndexType> master_conditions_ids_buffer;

        #pragma omp for
        for(int i=0; i<static_cast<int>(rModelPart.Nodes().size()); ++i) {
            auto it_node = rModelPart.NodesBegin() + i;

            if (it_node->Is(SLAVE) == !mInvertedSearch) {
                slave_nodes_ids_buffer.push_back(it_node->Id());
            }
            if (it_node->Is(MASTER) == !mInvertedSearch) {
                master_nodes_ids_buffer.push_back(it_node->Id());
            }
        }

        #pragma omp for
        for(int i=0; i<static_cast<int>(rModelPart.Conditions().size()); ++i) {
            auto it_cond = rModelPart.ConditionsBegin() + i;

            if (it_cond->Is(SLAVE) == !mInvertedSearch) {
                slave_conditions_ids_buffer.push_back(it_cond->Id());
            }
            if (it_cond->Is(MASTER) == !mInvertedSearch) {
                master_conditions_ids_buffer.push_back(it_cond->Id());
            }
        }

        // Combine buffers together
        #pragma omp critical
        {
            std::move(slave_nodes_ids_buffer.begin(),slave_nodes_ids_buffer.end(),back_inserter(slave_nodes_ids));
            std::move(master_nodes_ids_buffer.begin(),master_nodes_ids_buffer.end(),back_inserter(master_nodes_ids));
            std::move(slave_conditions_ids_buffer.begin(),slave_conditions_ids_buffer.end(),back_inserter(slave_conditions_ids));
            std::move(master_conditions_ids_buffer.begin(),master_conditions_ids_buffer.end(),back_inserter(master_conditions_ids));
        }
    }

    // Finally we add the nodes and conditions to the submodelparts
    r_slave_model_part.AddNodes(slave_nodes_ids);
    r_slave_model_part.AddConditions(slave_conditions_ids);
    r_master_model_part.AddNodes(master_nodes_ids);
    r_master_model_part.AddConditions(master_conditions_ids);

    KRATOS_ERROR_IF(r_master_model_part.Conditions().size() == 0) << "No origin conditions. Check your flags are properly set" << std::endl;
    KRATOS_ERROR_IF(r_slave_model_part.Conditions().size() == 0) << "No destination conditions. Check your flags are properly set" << std::endl;
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::ClearMortarConditions()
{
    ResetContactOperators();

    NodesArrayType& nodes_array = mrMainModelPart.GetSubModelPart("Contact").Nodes();

    switch(mTypeSolution) {
        case TypeSolution::VectorLagrangeMultiplier :
            ClearComponentsMortarConditions(nodes_array);
            break;
        case TypeSolution::ScalarLagrangeMultiplier :
            ClearScalarMortarConditions(nodes_array);
            break;
        case TypeSolution::NormalContactStress :
            ClearALMFrictionlessMortarConditions(nodes_array);
            break;
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::CreatePointListMortar()
{
    // Clearing the vector
    mPointListDestination.clear();

    // Iterate in the conditions
    ConditionsArrayType& conditions_array = mrMainModelPart.GetSubModelPart("Contact").Conditions();

    #pragma omp parallel
    {
        // Creating a buffer for parallel vector fill
        PointVector points_buffer;

        #pragma omp for
        for(int i = 0; i < static_cast<int>(conditions_array.size()); ++i) {
            auto it_cond = conditions_array.begin() + i;

            if (it_cond->Is(MASTER) == !mInvertedSearch || !mPredefinedMasterSlave) {
                const PointTypePointer& p_point = Kratos::make_shared<PointItem>((*it_cond.base()));
                (points_buffer).push_back(p_point);
            }
        }

        // Combine buffers together
        #pragma omp critical
        {
            std::move(points_buffer.begin(),points_buffer.end(),back_inserter(mPointListDestination));
        }
    }

#ifdef KRATOS_DEBUG
    // NOTE: We check the list
    for (IndexType i_point = 0; i_point < mPointListDestination.size(); ++i_point )
        mPointListDestination[i_point]->Check();
//     KRATOS_INFO("Check list") << "The list is properly built" << std::endl;
#endif
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::UpdatePointListMortar()
{
    // We check if we are in a dynamic or static case
    const bool dynamic = mThisParameters["dynamic_search"].GetBool() ? mrMainModelPart.NodesBegin()->SolutionStepsDataHas(VELOCITY_X) : false;
    const double delta_time = (dynamic) ? mrMainModelPart.GetProcessInfo()[DELTA_TIME] : 0.0;

    // We compute the delta displacement
    if (dynamic)
        ContactUtilities::ComputeStepJump(mrMainModelPart.GetSubModelPart("Contact"), delta_time);

    if (mCheckGap == CheckGap::MappingCheck && dynamic) {
        NodesArrayType& update_nodes_array = mrMainModelPart.GetSubModelPart("Contact").Nodes();

        #pragma omp parallel for
        for(int i = 0; i < static_cast<int>(update_nodes_array.size()); ++i)
            noalias((update_nodes_array.begin() + i)->Coordinates()) += (update_nodes_array.begin() + i)->GetValue(DELTA_COORDINATES);
    }

    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(mPointListDestination.size()); ++i)
        mPointListDestination[i]->UpdatePoint();
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::UpdateMortarConditions()
{
    // We update the list of points
    UpdatePointListMortar();

    // Calculate the mean of the normal in all the nodes
    MortarUtilities::ComputeNodesMeanNormalModelPart(mrMainModelPart.GetSubModelPart("Contact"));

    // We get the computing model part
    IndexType condition_id = ReorderConditionsIds();
    ModelPart& computing_rcontact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");

    // We reset the computing contact model part in case of already initialized
    if (computing_rcontact_model_part.Conditions().size() > 0)
        ClearMortarConditions();

    // We check if we are in a dynamic or static case
    const bool dynamic = mThisParameters["dynamic_search"].GetBool() ? mrMainModelPart.NodesBegin()->SolutionStepsDataHas(VELOCITY_X) : false;

    // Some auxiliar values
    const IndexType allocation_size = mThisParameters["allocation_size"].GetInt();           // Allocation size for the vectors and max number of potential results
    const double search_factor = mThisParameters["search_factor"].GetDouble();                  // The search factor to be considered
    SearchTreeType type_search = ConvertSearchTree(mThisParameters["type_search"].GetString()); // The search tree considered
    IndexType bucket_size = mThisParameters["bucket_size"].GetInt();                         // Bucket size for kd-tree

    // Create a tree
    // It will use a copy of mNodeList (a std::vector which contains pointers)
    // Copying the list is required because the tree will reorder it for efficiency
    KDTree tree_points(mPointListDestination.begin(), mPointListDestination.end(), bucket_size);

    // Auxiliar model parts and components
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ConditionsArrayType& conditions_array = rcontact_model_part.Conditions();
    const int num_conditions = static_cast<int>(conditions_array.size());

    // In case of not predefined master/slave we reset the flags
    if (!mPredefinedMasterSlave) {
        NodesArrayType& nodes_array = rcontact_model_part.Nodes();
        VariableUtils().SetFlag(SLAVE, false, nodes_array);
        VariableUtils().SetFlag(MASTER, false, nodes_array);
        VariableUtils().SetFlag(SLAVE, false, conditions_array);
        VariableUtils().SetFlag(MASTER, false, conditions_array);
    }

    // Now we iterate over the conditions
//     #pragma omp parallel for firstprivate(tree_points) // FIXME: Make me parallel!!!
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = conditions_array.begin() + i;

        if (!mPredefinedMasterSlave || it_cond->Is(SLAVE) == !mInvertedSearch) {
            // Initialize values
            PointVector points_found(allocation_size);

            IndexType number_points_found = 0;

            if (type_search == SearchTreeType::KdtreeInRadius) {
                GeometryType& geometry = it_cond->GetGeometry();
                const Point& center = dynamic ? ContactUtilities::GetHalfJumpCenter(geometry) : geometry.Center(); // NOTE: Center in half delta time or real center

                const double search_radius = search_factor * Radius(it_cond->GetGeometry());

                number_points_found = tree_points.SearchInRadius(center, search_radius, points_found.begin(), allocation_size);
            } else if (type_search == SearchTreeType::KdtreeInBox) {
                // Auxiliar values
                const double length_search = search_factor * it_cond->GetGeometry().Length();

                // Compute max/min points
                NodeType min_point, max_point;
                it_cond->GetGeometry().BoundingBox(min_point, max_point);

                // Get the normal in the extrema points
                Vector N_min, N_max;
                GeometryType::CoordinatesArrayType local_point_min, local_point_max;
                it_cond->GetGeometry().PointLocalCoordinates( local_point_min, min_point.Coordinates( ) ) ;
                it_cond->GetGeometry().PointLocalCoordinates( local_point_max, max_point.Coordinates( ) ) ;
                it_cond->GetGeometry().ShapeFunctionsValues( N_min, local_point_min );
                it_cond->GetGeometry().ShapeFunctionsValues( N_max, local_point_max );

                const array_1d<double,3> normal_min = MortarUtilities::GaussPointUnitNormal(N_min, it_cond->GetGeometry());
                const array_1d<double,3> normal_max = MortarUtilities::GaussPointUnitNormal(N_max, it_cond->GetGeometry());

                ContactUtilities::ScaleNode<NodeType>(min_point, normal_min, length_search);
                ContactUtilities::ScaleNode<NodeType>(max_point, normal_max, length_search);

                number_points_found = tree_points.SearchInBox(min_point, max_point, points_found.begin(), allocation_size);
            } else
                KRATOS_ERROR << " The type search is not implemented yet does not exist!!!!. SearchTreeType = " << mThisParameters["type_search"].GetString() << std::endl;

            if (number_points_found > 0) {
                // We resize the vector to the actual size
//                 points_found.resize(number_points_found); // NOTE: May be ineficient

            #ifdef KRATOS_DEBUG
                // NOTE: We check the list
                for (IndexType i_point = 0; i_point < number_points_found; ++i_point )
                    points_found[i_point]->Check();
//                 KRATOS_INFO("Check search") << "The search is properly done" << std::endl;
            #endif

                IndexSet::Pointer indexes_set = it_cond->GetValue(INDEX_SET);

                // If not active we check if can be potentially in contact
                if (mCheckGap == CheckGap::MappingCheck) {
                    for (IndexType i_point = 0; i_point < number_points_found; ++i_point ) {
                        Condition::Pointer p_cond_master = points_found[i_point]->GetCondition();
                        const CheckResult condition_checked_right = CheckCondition(indexes_set, (*it_cond.base()), p_cond_master, mInvertedSearch);
                        if (condition_checked_right == CheckResult::OK) indexes_set->AddId(p_cond_master->Id());
                    }
                } else
                    AddPotentialPairing(computing_rcontact_model_part, condition_id, (*it_cond.base()), points_found, number_points_found, indexes_set);
            }
        }
    }

    // In case of not predefined master/slave we assign the master/slave nodes and conditions
    if (!mPredefinedMasterSlave)
        NotPredefinedMasterSlave(rcontact_model_part);

    // We create the submodelparts for master and slave
    SetOriginDestinationModelParts(rcontact_model_part);

    // We map the Coordinates to the slave side from the master
    if (mCheckGap == CheckGap::MappingCheck)
        CheckPairing(computing_rcontact_model_part, condition_id);
    else {
        // We revert the nodes to the original position
        if (mThisParameters["dynamic_search"].GetBool()) {
            if (mrMainModelPart.NodesBegin()->SolutionStepsDataHas(VELOCITY_X)) {
                NodesArrayType& nodes_array = mrMainModelPart.GetSubModelPart("Contact").Nodes();
                #pragma omp parallel for
                for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i)
                    noalias((nodes_array.begin() + i)->Coordinates()) -= (nodes_array.begin() + i)->GetValue(DELTA_COORDINATES);
            }
        }
        // We compute the weighted reaction
        ComputeWeightedReaction();
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::AddPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId,
    Condition::Pointer pCondSlave,
    Condition::Pointer pCondMaster
    )
{
    if (mCreateAuxiliarConditions) { // We add the ID and we create a new auxiliar condition
        ++rConditionId;
        Condition::Pointer p_auxiliar_condition = rComputingModelPart.CreateNewCondition(mConditionName, rConditionId, pCondSlave->GetGeometry(), pCondSlave->pGetProperties());
        // We set the geometrical values
        p_auxiliar_condition->SetValue(PAIRED_GEOMETRY, pCondMaster->pGetGeometry());
        p_auxiliar_condition->SetValue(NORMAL, pCondSlave->GetValue(NORMAL));
        p_auxiliar_condition->SetValue(PAIRED_NORMAL, pCondMaster->GetValue(NORMAL));
        // We activate the condition and initialize it
        p_auxiliar_condition->Set(ACTIVE, true);
        p_auxiliar_condition->Initialize();
        if (mThisParameters["double_formulation"].GetBool()) {
            ++rConditionId;
            Condition::Pointer p_auxiliar_condition = rComputingModelPart.CreateNewCondition(mConditionName, rConditionId, pCondMaster->GetGeometry(), pCondMaster->pGetProperties());
            // We set the geometrical values
            p_auxiliar_condition->SetValue(PAIRED_GEOMETRY, pCondSlave->pGetGeometry());
            p_auxiliar_condition->SetValue(NORMAL, pCondMaster->GetValue(NORMAL));
            p_auxiliar_condition->SetValue(PAIRED_NORMAL, pCondSlave->GetValue(NORMAL));
            // We activate the condition and initialize it
            p_auxiliar_condition->Set(ACTIVE, true);
            p_auxiliar_condition->Initialize();
        }
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::AddPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId,
    Condition::Pointer pCondSlave,
    Condition::Pointer pCondMaster,
    IndexSet::Pointer IndexesSet
    )
{
    IndexesSet->AddId(pCondMaster->Id());

    AddPairing(rComputingModelPart, rConditionId, pCondSlave, pCondMaster);
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::CheckMortarConditions()
{
    // Iterate in the conditions
    ConditionsArrayType& conditions_array = mrMainModelPart.GetSubModelPart("Contact").Conditions();

    for(int i = 0; i < static_cast<int>(conditions_array.size()); ++i) {
        auto it_cond = conditions_array.begin() + i;

        if (it_cond->Has(INDEX_SET)) {
            IndexSet::Pointer ids_destination = it_cond->GetValue(INDEX_SET);
            if (ids_destination->size() > 0) {
                KRATOS_INFO("Check paired conditions (Origin)") << "Origin condition ID:" << it_cond->Id() << " Number of pairs: " << ids_destination->size() << std::endl;
                KRATOS_INFO("Check paired conditions (Destination)") << ids_destination->Info();
            }
        }
    }

    NodesArrayType& nodes_array = mrMainModelPart.GetSubModelPart("Contact").Nodes();

    for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
        auto it_node = nodes_array.begin() + i;
        if (it_node->Is(ACTIVE))
            KRATOS_INFO("Check paired nodes") << "Node: " << it_node->Id() << " is active" << std::endl;
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::InvertSearch()
{
    mInvertedSearch = !mInvertedSearch;
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::ClearScalarMortarConditions(NodesArrayType& NodesArray)
{
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(NodesArray.size()); ++i) {
        auto it_node = NodesArray.begin() + i;
        if (it_node->Is(ACTIVE) == false)
            it_node->FastGetSolutionStepValue(SCALAR_LAGRANGE_MULTIPLIER) = 0.0;
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::ClearComponentsMortarConditions(NodesArrayType& NodesArray)
{
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(NodesArray.size()); ++i) {
        auto it_node = NodesArray.begin() + i;
        if (it_node->Is(ACTIVE) == false)
            noalias((NodesArray.begin() + i)->FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) = ZeroVector(3);
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::ClearALMFrictionlessMortarConditions(NodesArrayType& NodesArray)
{
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(NodesArray.size()); ++i) {
        auto it_node = NodesArray.begin() + i;
        if (it_node->Is(ACTIVE) == false)
            (NodesArray.begin() + i)->FastGetSolutionStepValue(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE) = 0.0;
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::ComputeLinearRegressionGapPressure(
    double& a,
    double& b
    )
{
    // Initialize the n
    SizeType n = 0;

    // Initialize the values
    double xi, yi;
    double sum_x, sum_xsq, sum_y, sum_xy;

    sum_x = 0.0;
    sum_xsq = 0.0;
    sum_y = 0.0;
    sum_xy = 0.0;

    // Iterate over the nodes
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    NodesArrayType& nodes_array = rcontact_model_part.Nodes();

    // We compute now the normal gap and set the nodes under certain threshold as active
    #pragma omp parallel for private(xi, yi) reduction(+:sum_x,sum_xsq,sum_y,sum_xy,n)
    for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
        auto it_node = nodes_array.begin() + i;

        if (it_node->Is(ACTIVE)) {
            xi = it_node->FastGetSolutionStepValue(WEIGHTED_GAP);
            if (mTypeSolution == TypeSolution::NormalContactStress) {
                yi = it_node->FastGetSolutionStepValue(NORMAL_CONTACT_STRESS);
            } else {
                const array_1d<double, 3>& lm = it_node->FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER);
                const array_1d<double, 3>& normal = it_node->FastGetSolutionStepValue(NORMAL);
                yi = inner_prod(lm, normal);
            }
            sum_x += xi;
            sum_xsq += std::pow(xi, 2);
            sum_y += yi;
            sum_xy += xi * yi;
            n += 1;
        }
    }

    const double size = static_cast<double>(n);
    const double denom = size * sum_xsq - std::pow(sum_x, 2);
    a = (sum_y * sum_xsq - sum_x * sum_xy) / denom;
    b = (size * sum_xy - sum_x * sum_y) / denom;
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline double TreeContactSearch<TDim, TNumNodes>::GetMaxNodalH()
{
    // We iterate over the nodes
    NodesArrayType& nodes_array = mrMainModelPart.Nodes();

    // Creating a buffer for parallel vector fill
    const int num_threads = OpenMPUtils::GetNumThreads();
    std::vector<double> max_vector(num_threads, 0.0);
    double nodal_h;
    #pragma omp parallel for private(nodal_h)
    for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
        auto it_node = nodes_array.begin() + i;
        KRATOS_DEBUG_ERROR_IF_NOT(it_node->SolutionStepsDataHas(NODAL_H)) << "ERROR:: NODAL_H not added" << std::endl;
        nodal_h = it_node->FastGetSolutionStepValue(NODAL_H);

        const int id = OpenMPUtils::ThisThread();

        if (nodal_h > max_vector[id])
            max_vector[id] = nodal_h;
    }

    return *std::max_element(max_vector.begin(), max_vector.end());
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline double TreeContactSearch<TDim, TNumNodes>::GetMeanNodalH()
{
    // We iterate over the nodes
    NodesArrayType& nodes_array = mrMainModelPart.Nodes();

    double nodal_h;
    double sum_nodal_h = 0.0;

    #pragma omp parallel for  private(nodal_h) reduction(+:sum_nodal_h)
    for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
        auto it_node = nodes_array.begin() + i;
        KRATOS_DEBUG_ERROR_IF_NOT(it_node->SolutionStepsDataHas(NODAL_H)) << "ERROR:: NODAL_H not added" << std::endl;
        nodal_h = it_node->FastGetSolutionStepValue(NODAL_H);
        sum_nodal_h += nodal_h;
    }

    return sum_nodal_h/static_cast<double>(nodes_array.size());
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline typename TreeContactSearch<TDim, TNumNodes>::CheckResult TreeContactSearch<TDim, TNumNodes>::CheckCondition(
    IndexSet::Pointer IndexesSet,
    const Condition::Pointer pCond1,
    const Condition::Pointer pCond2,
    const bool InvertedSearch
    )
{
    const IndexType index_1 = pCond1->Id();
    const IndexType index_2 = pCond2->Id();

    if (index_1 == index_2) // Avoiding "auto self-contact"
        return CheckResult::Fail;

    // Avoid conditions oriented in the same direction
    const double tolerance = 1.0e-16;
    if (norm_2(pCond1->GetValue(NORMAL) - pCond2->GetValue(NORMAL)) < tolerance)
        return CheckResult::Fail;

    // Otherwise will not be necessary to check
    if (!mPredefinedMasterSlave || pCond2->Is(SLAVE) == !InvertedSearch) {
        auto& indexes_set_2 = pCond2->GetValue(INDEX_SET);
        if (indexes_set_2->find(index_1) != indexes_set_2->end())
            return CheckResult::Fail;
    }

    // To avoid to repeat twice the same condition
    if (IndexesSet->find(index_2) != IndexesSet->end())
        return CheckResult::AlreadyInTheMap;

    return CheckResult::OK;
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::NotPredefinedMasterSlave(ModelPart& rModelPart)
{
    // We iterate over the conditions
    ConditionsArrayType& conditions_array = rModelPart.Conditions();
    const int num_conditions = static_cast<int>(conditions_array.size());

    std::vector<IndexType> master_conditions_ids;

    #pragma omp parallel
    {
        // Creating a buffer for parallel vector fill
        std::vector<IndexType> master_conditions_ids_buffer;

        #pragma omp for
        for(int i = 0; i < num_conditions; ++i) {
            auto it_cond = conditions_array.begin() + i;
            IndexSet::Pointer indexes_set = it_cond->GetValue(INDEX_SET);
            if (indexes_set->size() > 0) {
                it_cond->Set(SLAVE, true);
                for (auto& i_pair : *indexes_set) {
                    master_conditions_ids_buffer.push_back(i_pair);
                }
            }
        }

        // Combine buffers together
        #pragma omp critical
        {
            std::move(master_conditions_ids_buffer.begin(),master_conditions_ids_buffer.end(),back_inserter(master_conditions_ids));
        }
    }

    // We create an auxiliar model part to add the MASTER flag
    rModelPart.CreateSubModelPart("AuxMasterModelPart");
    ModelPart& aux_model_part = rModelPart.GetSubModelPart("AuxMasterModelPart");

    // Remove duplicates
    std::sort( master_conditions_ids.begin(), master_conditions_ids.end() );
    master_conditions_ids.erase( std::unique( master_conditions_ids.begin(), master_conditions_ids.end() ), master_conditions_ids.end() );

    // Add to the auxiliar model part
    aux_model_part.AddConditions(master_conditions_ids);

    // Set the flag
    VariableUtils().SetFlag(MASTER, true, aux_model_part.Conditions());

    // Remove auxiliar model part
    rModelPart.RemoveSubModelPart("AuxMasterModelPart");

    // Now we iterate over the conditions to set the nodes indexes
    #pragma omp parallel for
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = conditions_array.begin() + i;
        if (it_cond->Is(SLAVE)) {
            GeometryType& this_geometry = it_cond->GetGeometry();
            for (auto& node : this_geometry) {
                node.SetLock();
                node.Set(SLAVE, true);
                node.UnSetLock();
            }
        }
        if (it_cond->Is(MASTER)) {
            GeometryType& this_geometry = it_cond->GetGeometry();
            for (auto& node : this_geometry) {
                node.SetLock();
                node.Set(MASTER, true);
                node.UnSetLock();
            }
        }
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline IndexType TreeContactSearch<TDim, TNumNodes>::ReorderConditionsIds()
{
    ConditionsArrayType& conditions_array = mrMainModelPart.Conditions();

    IndexType condition_id = 0;
    for(int i = 0; i < static_cast<int>(conditions_array.size()); ++i)  {
        ++condition_id;
        (conditions_array.begin() + i)->SetId(condition_id);
    }

    return condition_id;
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::AddPotentialPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId,
    Condition::Pointer pCondSlave,
    PointVector& rPointsFound,
    const IndexType NumberOfPointsFound,
    IndexSet::Pointer IndexesSet
    )
{
    // Some auxiliar values
    const double active_check_factor = mrMainModelPart.GetProcessInfo()[ACTIVE_CHECK_FACTOR];
    const double tolerance = std::numeric_limits<double>::epsilon();
    const bool frictional = mrMainModelPart.IsDefined(SLIP) ? mrMainModelPart.Is(SLIP) : false;

    // Slave geometry
    GeometryType& geom_slave = pCondSlave->GetGeometry();
    const array_1d<double, 3>& normal_slave = pCondSlave->GetValue(NORMAL);

    for (IndexType i_point = 0; i_point < NumberOfPointsFound; ++i_point ) {
        bool at_least_one_node_potential_contact = false;

        // Master condition
        Condition::Pointer p_cond_master = rPointsFound[i_point]->GetCondition();

        if (mCheckGap == CheckGap::DirectCheck) {
            // Master geometry
            const array_1d<double, 3>& normal_master = p_cond_master->GetValue(NORMAL);
            GeometryType& geom_master = p_cond_master->GetGeometry();

            for (IndexType i_node = 0; i_node < TNumNodes; ++i_node) {
                if (geom_slave[i_node].Is(ACTIVE) == false) {
                    Point projected_point;
                    double aux_distance = 0.0;
                    const array_1d<double, 3> normal = geom_slave[i_node].GetValue(NORMAL);
                    if (norm_2(normal) < tolerance)
                        aux_distance = MortarUtilities::FastProjectDirection(geom_master, geom_slave[i_node], projected_point, normal_master, normal_slave);
                    else
                        aux_distance = MortarUtilities::FastProjectDirection(geom_master, geom_slave[i_node], projected_point, normal_master, normal);

                    array_1d<double, 3> result;
                    if (aux_distance <= geom_slave[i_node].FastGetSolutionStepValue(NODAL_H) * active_check_factor &&  geom_master.IsInside(projected_point, result, tolerance)) { // NOTE: This can be problematic (It depends the way IsInside() and the local_pointCoordinates() are implemented)
                        at_least_one_node_potential_contact = true;
                        geom_slave[i_node].Set(ACTIVE, true);
                        if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && frictional)
                            if (norm_2(geom_slave[i_node].FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance)
                                geom_slave[i_node].Set(SLIP, false);
                    }

                    aux_distance = MortarUtilities::FastProjectDirection(geom_master, geom_slave[i_node], projected_point, normal_master, -normal_master);
                    if (aux_distance <= geom_slave[i_node].FastGetSolutionStepValue(NODAL_H) * active_check_factor &&  geom_master.IsInside(projected_point, result, tolerance)) { // NOTE: This can be problematic (It depends the way IsInside() and the local_pointCoordinates() are implemented)
                        at_least_one_node_potential_contact = true;
                        geom_slave[i_node].Set(ACTIVE, true);
                        if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && frictional)
                            if (norm_2(geom_slave[i_node].FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance)
                                geom_slave[i_node].Set(SLIP, false);
                    }
                } else
                    at_least_one_node_potential_contact = true;
            }
            if (mThisParameters["double_formulation"].GetBool()) {
                for (IndexType i_node = 0; i_node < TNumNodes; ++i_node) {
                    if (geom_master[i_node].Is(ACTIVE) == false) {
                        Point projected_point;
                        double aux_distance = 0.0;
                        const array_1d<double, 3> normal = geom_master[i_node].GetValue(NORMAL);
                        if (norm_2(normal) < tolerance)
                            aux_distance = MortarUtilities::FastProjectDirection(geom_slave, geom_master[i_node], projected_point, normal_master, normal_master);
                        else
                            aux_distance = MortarUtilities::FastProjectDirection(geom_slave, geom_master[i_node], projected_point, normal_master, normal);

                        array_1d<double, 3> result;
                        if (aux_distance <= geom_master[i_node].FastGetSolutionStepValue(NODAL_H) * active_check_factor &&  geom_slave.IsInside(projected_point, result, tolerance)) { // NOTE: This can be problematic (It depends the way IsInside() and the local_pointCoordinates() are implemented)
                            at_least_one_node_potential_contact = true;
                            geom_master[i_node].Set(ACTIVE, true);
                            if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && frictional)
                                if (norm_2(geom_master[i_node].FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance)
                                    geom_master[i_node].Set(SLIP, false);
                        }

                        aux_distance = MortarUtilities::FastProjectDirection(geom_slave, geom_master[i_node], projected_point, normal_master, -normal_master);
                        if (aux_distance <= geom_master[i_node].FastGetSolutionStepValue(NODAL_H) * active_check_factor &&  geom_slave.IsInside(projected_point, result, tolerance)) { // NOTE: This can be problematic (It depends the way IsInside() and the local_pointCoordinates() are implemented)
                            at_least_one_node_potential_contact = true;
                            geom_master[i_node].Set(ACTIVE, true);
                            if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && frictional)
                                if (norm_2(geom_master[i_node].FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance)
                                    geom_master[i_node].Set(SLIP, false);
                        }
                    } else
                        at_least_one_node_potential_contact = true;
                }
            }
        } else {
            at_least_one_node_potential_contact = true;
            for (IndexType i_node = 0; i_node < TNumNodes; ++i_node) {
                geom_slave[i_node].Set(ACTIVE, true);
                if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && frictional)
                        if (norm_2(geom_slave[i_node].FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance)
                            geom_slave[i_node].Set(SLIP, false);
            }
        }

        if (at_least_one_node_potential_contact)
            AddPairing(rComputingModelPart, rConditionId, pCondSlave, p_cond_master, IndexesSet);
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CheckPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId
    )
{
    // We compute the maximal nodal h and some auxiliar values  // TODO: Think about this criteria
//     const double distance_threshold = 2.0/3.0 * GetMeanNodalH();
    const double distance_threshold = GetMeanNodalH();
//     const double distance_threshold = GetMaxNodalH();
//     const double distance_threshold = mrMainModelPart.GetProcessInfo()[ACTIVE_CHECK_FACTOR] * GetMaxNodalH();

    // Updating the distance distance threshold
    mrMainModelPart.GetProcessInfo().SetValue(DISTANCE_THRESHOLD, distance_threshold);

    // We get the contact model part
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");

    // We set the gap to an enormous value in order to initialize it
    VariableUtils().SetNonHistoricalScalarVar<Variable<double>>(NORMAL_GAP, 1.0e12, rcontact_model_part.Nodes());

    // We compute the gap in the slave
    ComputeMappedGap(!mInvertedSearch);
    if (mThisParameters["double_formulation"].GetBool())
        ComputeMappedGap(mInvertedSearch);

    // We revert the nodes to the original position
    NodesArrayType& nodes_array = rcontact_model_part.Nodes();
    if (mThisParameters["dynamic_search"].GetBool()) {
        if (mrMainModelPart.NodesBegin()->SolutionStepsDataHas(VELOCITY_X)) {
            #pragma omp parallel for
            for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
                auto it_node = nodes_array.begin() + i;
                noalias(it_node->Coordinates()) -= it_node->GetValue(DELTA_COORDINATES);
            }
        }
    }

    // Calculate the mean of the normal in all the nodes
    MortarUtilities::ComputeNodesMeanNormalModelPart(rcontact_model_part);

    // Iterate in the conditions and create the new ones
    CreateAuxiliarConditions(rcontact_model_part, rComputingModelPart, rConditionId);

    // We compute the weighted reaction
    ComputeWeightedReaction();

    // Finally we compute the active/inactive nodes
    ComputeActiveInactiveNodes();
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::ComputeMappedGap(const bool SearchOrientation)
{
    // Some auxiliar values
    const double tolerance = std::numeric_limits<double>::epsilon();
//     const double active_check_factor = mrMainModelPart.GetProcessInfo()[ACTIVE_CHECK_FACTOR];
    const double distance_threshold = mrMainModelPart.GetProcessInfo()[DISTANCE_THRESHOLD];

    // Iterate over the nodes
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_master_model_part = rcontact_model_part.GetSubModelPart("MasterSubModelPart");
    NodesArrayType& nodes_array_master = r_master_model_part.Nodes();
    ModelPart& r_slave_model_part = rcontact_model_part.GetSubModelPart("SlaveSubModelPart");
    NodesArrayType& nodes_array_slave = r_slave_model_part.Nodes();

    // We set the auxiliar Coordinates
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(nodes_array_master.size()); ++i) {
        auto it_node = nodes_array_master.begin() + i;

        if (SearchOrientation)
            it_node->SetValue(AUXILIAR_COORDINATES, it_node->Coordinates());
        else
            it_node->SetValue(AUXILIAR_COORDINATES, ZeroVector(3));
    }
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(nodes_array_slave.size()); ++i) {
        auto it_node = nodes_array_slave.begin() + i;

        if (!SearchOrientation)
            it_node->SetValue(AUXILIAR_COORDINATES, it_node->Coordinates());
        else
            it_node->SetValue(AUXILIAR_COORDINATES, ZeroVector(3));
    }

    // Switch MASTER/SLAVE
    NodesArrayType& nodes_array = rcontact_model_part.Nodes();
    if (!SearchOrientation)
        SwitchFlagNodes(nodes_array);

    // We set the mapper parameters
    Parameters mapping_parameters = Parameters(R"({"distance_threshold" : 1.0e24})" );
    mapping_parameters["distance_threshold"].SetDouble(distance_threshold);
    typedef SimpleMortarMapperProcess<TDim, TNumNodes, Variable<array_1d<double, 3>>, NonHistorical> MapperType;
    MapperType mapper = MapperType(r_master_model_part, r_slave_model_part, AUXILIAR_COORDINATES, mapping_parameters);
    mapper.Execute();

    // Switch again MASTER/SLAVE
    if (!SearchOrientation)
        SwitchFlagNodes(nodes_array);

    // We compute now the normal gap and set the nodes under certain threshold as active
    array_1d<double, 3> normal, auxiliar_coordinates, components_gap;
    double gap;
    #pragma omp parallel for private(gap, normal, auxiliar_coordinates, components_gap)
    for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
        auto it_node = nodes_array.begin() + i;

        if (it_node->Is(SLAVE) == SearchOrientation) {
            // We compute the gap
            normal = it_node->FastGetSolutionStepValue(NORMAL);
            auxiliar_coordinates = it_node->GetValue(AUXILIAR_COORDINATES);
            components_gap = ( it_node->Coordinates() - auxiliar_coordinates);
            gap = inner_prod(components_gap, - normal);

            // We activate if the node is close enough
            if (norm_2(auxiliar_coordinates) > tolerance)
                it_node->SetValue(NORMAL_GAP, gap);
        }
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::ComputeActiveInactiveNodes()
{
    // Some auxiliar values
    const double active_check_factor = mrMainModelPart.GetProcessInfo()[ACTIVE_CHECK_FACTOR];
    const double distance_threshold = mrMainModelPart.GetProcessInfo()[DISTANCE_THRESHOLD];

    // Compute linear regression
    double a, b;
    ComputeLinearRegressionGapPressure(a, b);

    // Iterate over the nodes
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    NodesArrayType& nodes_array = rcontact_model_part.Nodes();

    // We compute now the normal gap and set the nodes under certain threshold as active
    bool auxiliar_check;
    #pragma omp parallel for private(auxiliar_check)
    for(int i = 0; i < static_cast<int>(nodes_array.size()); ++i) {
        auto it_node = nodes_array.begin() + i;

        const double auxiliar_length = distance_threshold * active_check_factor;
        auxiliar_check = false;
        if (it_node->SolutionStepsDataHas(WEIGHTED_GAP)) {
            const double nodal_area = it_node->Has(NODAL_AREA) ? it_node->GetValue(NODAL_AREA) : 1.0;
            auxiliar_check = (it_node->FastGetSolutionStepValue(WEIGHTED_GAP)/nodal_area < auxiliar_length) ? true : false;
        }
        if ((it_node->GetValue(NORMAL_GAP) < auxiliar_length) || auxiliar_check)
            SetActiveNode(it_node, a, b);
        else
            SetInactiveNode(it_node);
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::SetActiveNode(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    if (ItNode->Is(ACTIVE) ) {
        switch(mTypeSolution) {
            case TypeSolution::VectorLagrangeMultiplier :
                if (mrMainModelPart.Is(SLIP))
                    CorrectALMFrictionalMortarLM(ItNode, a, b);
                else if (mrMainModelPart.Is(CONTACT))
                    CorrectALMFrictionlessComponentsMortarLM(ItNode, a, b);
                else
                    CorrectComponentsMortarLM(ItNode, a, b);
                break;
            case TypeSolution::ScalarLagrangeMultiplier :
                CorrectScalarMortarLM(ItNode, a, b);
                break;
            case TypeSolution::NormalContactStress :
                CorrectALMFrictionlessMortarLM(ItNode, a, b);
                break;
        }
    } else {
        ItNode->Set(ACTIVE, true);
        switch(mTypeSolution) {
            case TypeSolution::VectorLagrangeMultiplier :
                if (mrMainModelPart.Is(SLIP))
                    PredictALMFrictionalMortarLM(ItNode, a, b);
                else if (mrMainModelPart.Is(CONTACT))
                    PredictALMFrictionlessComponentsMortarLM(ItNode, a, b);
                else
                    PredictComponentsMortarLM(ItNode, a, b);
                break;
            case TypeSolution::ScalarLagrangeMultiplier :
                PredictScalarMortarLM(ItNode, a, b);
                break;
            case TypeSolution::NormalContactStress :
                PredictALMFrictionlessMortarLM(ItNode, a, b);
                break;
        }
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::SetInactiveNode(NodesArrayType::iterator ItNode)
{
    if (ItNode->Is(ACTIVE) ) {
        ItNode->Set(ACTIVE, false);
        switch(mTypeSolution) {
            case TypeSolution::VectorLagrangeMultiplier :
                noalias(ItNode->FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) = ZeroVector(3);
                break;
            case TypeSolution::ScalarLagrangeMultiplier :
                ItNode->FastGetSolutionStepValue(SCALAR_LAGRANGE_MULTIPLIER) = 0.0;
                break;
            case TypeSolution::NormalContactStress :
                ItNode->FastGetSolutionStepValue(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE) = 0.0;
                break;
        }
    }

    // We set the gap to zero (in order to have something "visible" to post process)
    ItNode->SetValue(NORMAL_GAP, 0.0);
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CorrectScalarMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CorrectComponentsMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CorrectALMFrictionlessMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
//     const double old_weighted_gap = ItNode->FastGetSolutionStepValue(WEIGHTED_GAP, 1);
    const double current_weighted_gap = ItNode->FastGetSolutionStepValue(WEIGHTED_GAP);

    double& current_contact_stress = ItNode->FastGetSolutionStepValue(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE);

    // Apply linear regression
    const double aux_press = a + current_weighted_gap * b;
    current_contact_stress = (aux_press < 0.0) ? aux_press : 0.0;

//     const bool old_penetration = (old_weighted_gap < 0.0) ? true : false;
//     const bool current_penetration = (current_weighted_gap < 0.0) ? true : false;
//
//     // If both are positive or negative we just interpolate the current values
//     if (old_penetration == current_penetration) {
//         if (old_penetration) { // Penetrating
//             if (std::abs(old_weighted_gap) > std::numeric_limits<double>::epsilon())
//                 current_contact_stress *= current_weighted_gap/old_weighted_gap;
//         } else { // Not penetrating
//             if (old_weighted_gap > std::numeric_limits<double>::epsilon())
//                 current_contact_stress /= current_weighted_gap/old_weighted_gap;
//         }
//     } else if (old_penetration && !current_penetration) { // We had penenetration and we don't have it anymore
//         const double gap_variation = (current_weighted_gap + old_weighted_gap);
//         if (std::abs(gap_variation) > std::numeric_limits<double>::epsilon()) {
//             if (gap_variation > 0.0)
//                 current_contact_stress *= - old_weighted_gap/gap_variation;
//             else
//                 current_contact_stress *= - current_weighted_gap/gap_variation;
//         }
//     } else { // We have penetration and we haven't before
//         const double gap_variation = (old_weighted_gap - current_weighted_gap);
//         if (std::abs(gap_variation) > std::numeric_limits<double>::epsilon()) {
//             if (gap_variation > 0.0)
//                 current_contact_stress /= - current_weighted_gap/gap_variation;
//             else
//                 current_contact_stress /= - old_weighted_gap/gap_variation;
//         }
//     }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CorrectALMFrictionlessComponentsMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CorrectALMFrictionalMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::PredictScalarMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::PredictComponentsMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::PredictALMFrictionlessMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
//     // The penalty to be use (TODO: think about use the nodal penalty)
//     ProcessInfo& current_process_info = mrMainModelPart.GetProcessInfo(); // TODO: Avoid call the process info each time
//     const double initial_penalty = current_process_info[INITIAL_PENALTY];
//     const double distance_threshold = current_process_info[DISTANCE_THRESHOLD];
//
//     // If we have penetration
    const double current_weighted_gap = ItNode->FastGetSolutionStepValue(WEIGHTED_GAP);
//     const double nodal_area = ItNode->GetValue(NODAL_AREA);
//     const bool current_penetration = (current_weighted_gap < 0.0) ? true : false;
//
    double& current_contact_stress = ItNode->FastGetSolutionStepValue(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE);

    // Apply linear regression
    const double aux_press = a + current_weighted_gap * b;
    current_contact_stress = (aux_press < 0.0) ? aux_press : 0.0;
//
//     // We have penetration so we just basically approximate the solution with the traditional
//     if (current_penetration) {
//         current_contact_stress = initial_penalty * current_weighted_gap;
//     } else { // We don't have penetration, we do a simpler approach
//         const double relative_gap = (current_weighted_gap - distance_threshold * nodal_area);
//         current_contact_stress = (relative_gap < 0.0) ? initial_penalty * relative_gap : 0.0;
//     }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::PredictALMFrictionlessComponentsMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
//     // The penalty to be use (TODO: think about use the nodal penalty)
//     ProcessInfo& current_process_info = mrMainModelPart.GetProcessInfo(); // TODO: Avoid call the process info each time
//     const double initial_penalty = current_process_info[INITIAL_PENALTY];
// //     const double distance_threshold = current_process_info[DISTANCE_THRESHOLD];
//
//     // If we have penetration
//     const double current_weighted_gap = ItNode->FastGetSolutionStepValue(WEIGHTED_GAP);
//     const double nodal_area = ItNode->GetValue(NODAL_AREA);
//     const bool current_penetration = (current_weighted_gap < 0.0) ? true : false;
//
//     if (current_penetration) {
//         const array_1d<double, 3>& normal = ItNode->FastGetSolutionStepValue(NORMAL);
//         const array_1d<double, 3> predicted_contact_stress = normal * initial_penalty * current_weighted_gap/nodal_area;
//         ItNode->FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER) = predicted_contact_stress;
//     }
//
//     // Apply linear regression
//     const double aux_press = a + current_weighted_gap * b;
//     current_contact_stress = (aux_press < 0.0) ? aux_press : 0.0;
//
//     // We have penetration so we just basically approximate the solution with the traditional
//     if (current_penetration) {
//         current_contact_stress = initial_penalty * current_weighted_gap;
//     } else { // We don't have penetration, we do a simpler approach
//         const double relative_gap = (current_weighted_gap - distance_threshold * nodal_area);
//         current_contact_stress = (relative_gap < 0.0) ? initial_penalty * relative_gap : 0.0;
//     }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::PredictALMFrictionalMortarLM(
    NodesArrayType::iterator ItNode,
    const double a,
    const double b
    )
{
    // TODO: Add correction
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::ComputeWeightedReaction()
{
    // Auxiliar zero array
    const array_1d<double, 3> zero_array(3, 0.0);

    // Auxiliar gap
    ModelPart& rcontact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    NodesArrayType& nodes_array = rcontact_model_part.Nodes();
    switch(mTypeSolution) {
        case TypeSolution::VectorLagrangeMultiplier :
            if (mrMainModelPart.Is(SLIP)) {
                VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, nodes_array);
                VariableUtils().SetVectorVar(WEIGHTED_SLIP, zero_array, nodes_array);
            } else if (mrMainModelPart.Is(CONTACT)) {
                VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, nodes_array);
            } else
                VariableUtils().SetVectorVar(WEIGHTED_VECTOR_RESIDUAL, zero_array, nodes_array);
            break;
        case TypeSolution::ScalarLagrangeMultiplier :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_SCALAR_RESIDUAL, 0.0, nodes_array);
            break;
        case TypeSolution::NormalContactStress :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, nodes_array);
            break;
    }
    ModelPart& r_computing_contact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
    ConditionsArrayType& computing_conditions_array = r_computing_contact_model_part.Conditions();
    auto process_info = mrMainModelPart.GetProcessInfo();
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(computing_conditions_array.size()); ++i) {
        auto it_cond = computing_conditions_array.begin() + i;
        it_cond->AddExplicitContribution(process_info);
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline void TreeContactSearch<TDim, TNumNodes>::CreateAuxiliarConditions(
    ModelPart& rContactModelPart,
    ModelPart& rComputingModelPart,
    IndexType& rConditionId
    )
{
    // Iterate in the conditions and create the new ones
    ConditionsArrayType& conditions_array = rContactModelPart.Conditions();

    for(IndexType i = 0; i < conditions_array.size(); ++i) {
        auto it_cond = conditions_array.begin() + i;
        if (it_cond->Is(SLAVE) == !mInvertedSearch) {
            IndexSet::Pointer indexes_set = it_cond->GetValue(INDEX_SET);
            for (auto it_pair = indexes_set->begin(); it_pair != indexes_set->end(); ++it_pair ) {
                Condition::Pointer p_cond_master = mrMainModelPart.pGetCondition(*it_pair); // MASTER
                AddPairing(rComputingModelPart, rConditionId, (*it_cond.base()), p_cond_master);
            }
        }
    }
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
inline double TreeContactSearch<TDim, TNumNodes>::Radius(GeometryType& ThisGeometry)
{
    double radius = 0.0;
    const Point& center = ThisGeometry.Center();

    for(IndexType i_node = 0; i_node < ThisGeometry.PointsNumber(); ++i_node)  {
        const array_1d<double, 3>& aux_vector = center.Coordinates() - ThisGeometry[i_node].Coordinates();
        const double aux_value = inner_prod(aux_vector, aux_vector);
        if(aux_value > radius) radius = aux_value;
    }

    return std::sqrt(radius);
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
void TreeContactSearch<TDim, TNumNodes>::ResetContactOperators()
{
    ConditionsArrayType& conditions_array = mrMainModelPart.GetSubModelPart("Contact").Conditions();

    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(conditions_array.size()); ++i) {
        auto it_cond = conditions_array.begin() + i;
        if (it_cond->Is(SLAVE) == !mInvertedSearch) {
            auto& condition_pointers = it_cond->GetValue(INDEX_SET);

            if (condition_pointers != nullptr) {
                condition_pointers->clear();
//                     condition_pointers->reserve(mAllocationSize);
            }
        }
    }

    ModelPart& computing_rcontact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
    ConditionsArrayType& computing_conditions_array = computing_rcontact_model_part.Conditions();
    const int num_computing_conditions = static_cast<int>(computing_conditions_array.size());

    #pragma omp parallel for
    for(int i = 0; i < num_computing_conditions; ++i)
        (computing_conditions_array.begin() + i)->Set(TO_ERASE, true);

    mrMainModelPart.RemoveConditionsFromAllLevels(TO_ERASE);
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
typename TreeContactSearch<TDim, TNumNodes>::SearchTreeType TreeContactSearch<TDim, TNumNodes>::ConvertSearchTree(const std::string& str)
{
    KRATOS_ERROR_IF(str == "KDOP") << "KDOP contact search: Not yet implemented" << std::endl;

    if(str == "InRadius" || str == "in_radius")
        return SearchTreeType::KdtreeInRadius;
    else if(str == "InBox" || str == "in_box")
        return SearchTreeType::KdtreeInBox;
    else if (str == "KDOP" || str == "kdop")
        return SearchTreeType::Kdop;
    else
        return SearchTreeType::KdtreeInRadius;
}

/***********************************************************************************/
/***********************************************************************************/

template<std::size_t TDim, std::size_t TNumNodes>
typename TreeContactSearch<TDim, TNumNodes>::CheckGap TreeContactSearch<TDim, TNumNodes>::ConvertCheckGap(const std::string& str)
{
    if(str == "NoCheck" || str == "no_check")
        return CheckGap::NoCheck;
    else if(str == "DirectCheck" || str == "direct_check")
        return CheckGap::DirectCheck;
    else if (str == "CheckGap::MappingCheck" || str == "mapping_check")
        return CheckGap::MappingCheck;
    else
        return CheckGap::MappingCheck;
}

/***********************************************************************************/
/***********************************************************************************/

template class TreeContactSearch<2, 2>;
template class TreeContactSearch<3, 3>;
template class TreeContactSearch<3, 4>;

}  // namespace Kratos.
