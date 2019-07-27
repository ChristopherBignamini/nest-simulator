/*
 *  parameter.cpp
 *
 *  This file is part of NEST.
 *
 *  Copyright (C) 2004 The NEST Initiative
 *
 *  NEST is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  NEST is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NEST.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "gid_collection.h"
#include "node.h"
#include "topology.h"

// includes from sli
<<<<<<< Updated upstream
#include "sharedptrdatum.h"

#include "parameter.h"

template class sharedPtrDatum< nest::Parameter, &nest::NestModule::ParameterType >;
=======
#include "lockptrdatum_impl.h"

#include "parameter.h"

template class lockPTRDatum< nest::Parameter, &nest::NestModule::ParameterType >;
>>>>>>> Stashed changes

namespace nest
{
Node*
Parameter::gid_to_node_ptr_( const index gid, const thread t ) const
{
  return kernel().node_manager.get_node_or_proxy( gid, t );
}


double
NodePosParameter::get_node_pos_( librandom::RngPtr& rng, Node* node ) const
{
  if ( not node )
  {
    throw KernelException( "NodePosParameter: not node" );
  }
  GIDCollectionPTR gc = node->get_gc();
<<<<<<< Updated upstream
  if ( not gc.get() )
=======
  if ( not gc.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "NodePosParameter: not gc" );
  }
  GIDCollectionMetadataPTR meta = gc->get_metadata();
<<<<<<< Updated upstream
  if ( not meta.get() )
=======
  if ( not meta.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "NodePosParameter: not meta" );
  }
  LayerMetadata const* const layer_meta = dynamic_cast< LayerMetadata const* >( meta.get() );
<<<<<<< Updated upstream
=======
  meta.unlock();
>>>>>>> Stashed changes
  if ( not layer_meta )
  {
    throw KernelException( "NodePosParameter: not layer_meta" );
  }
  AbstractLayerPTR layer = layer_meta->get_layer();
<<<<<<< Updated upstream
  if ( not layer.get() )
=======
  if ( not layer.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "NodePosParameter: not valid layer" );
  }
  index lid = node->get_gid() - meta->get_first_gid();
  std::vector< double > pos = layer->get_position_vector( lid );
  if ( ( unsigned int ) dimension_ >= pos.size() )
  {
    throw KernelException(
      "Node position dimension must be within the defined number of "
      "dimensions for the node." );
  }
  return pos[ dimension_ ];
}

double
SpatialDistanceParameter::value( librandom::RngPtr& rng, index sgid, Node* target, thread target_thread ) const
{
  Node* source = gid_to_node_ptr_( sgid, target_thread );
  // Initial checks
  if ( not source )
  {
    throw KernelException( "SpatialDistanceParameter: source not node" );
  }
  if ( not target )
  {
    throw KernelException( "SpatialDistanceParameter: target not node" );
  }

  // Source

  GIDCollectionPTR source_gc = source->get_gc();
<<<<<<< Updated upstream
  if ( not source_gc.get() )
=======
  if ( not source_gc.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "SpatialDistanceParameter: not source gc" );
  }
  GIDCollectionMetadataPTR source_meta = source_gc->get_metadata();
<<<<<<< Updated upstream
  if ( not source_meta.get() )
=======
  if ( not source_meta.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "SpatialDistanceParameter: not source meta" );
  }
  LayerMetadata const* const source_layer_meta = dynamic_cast< LayerMetadata const* >( source_meta.get() );
<<<<<<< Updated upstream
=======
  source_meta.unlock();
>>>>>>> Stashed changes
  if ( not source_layer_meta )
  {
    throw KernelException( "SpatialDistanceParameter: not source_layer_meta" );
  }
  AbstractLayerPTR source_layer = source_layer_meta->get_layer();
<<<<<<< Updated upstream
  if ( not source_layer.get() )
=======
  if ( not source_layer.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "SpatialDistanceParameter: not valid source layer" );
  }
  index source_lid = source->get_gid() - source_meta->get_first_gid();
  // std::vector< double > pos = source_layer->get_position_vector( source_lid
  // );

  // Target

  GIDCollectionPTR target_gc = target->get_gc();
<<<<<<< Updated upstream
  if ( not target_gc.get() )
=======
  if ( not target_gc.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "SpatialDistanceParameter: not target gc" );
  }
  GIDCollectionMetadataPTR target_meta = target_gc->get_metadata();
<<<<<<< Updated upstream
  if ( not target_meta.get() )
=======
  if ( not target_meta.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "SpatialDistanceParameter: not target meta" );
  }
  LayerMetadata const* const target_layer_meta = dynamic_cast< LayerMetadata const* >( target_meta.get() );
<<<<<<< Updated upstream
=======
  target_meta.unlock();
>>>>>>> Stashed changes
  if ( not target_layer_meta )
  {
    throw KernelException( "SpatialDistanceParameter: not target_layer_meta" );
  }
  AbstractLayerPTR target_layer = target_layer_meta->get_layer();
<<<<<<< Updated upstream
  if ( not target_layer.get() )
=======
  if ( not target_layer.valid() )
>>>>>>> Stashed changes
  {
    throw KernelException( "SpatialDistanceParameter: not valid target layer" );
  }
  index target_lid = target->get_gid() - target_meta->get_first_gid();
  std::vector< double > target_pos = target_layer->get_position_vector( target_lid );

  switch ( dimension_ )
  {
  case 0:
    return source_layer->compute_distance( target_pos, source_lid );
  case 1:
  case 2:
  case 3:
    if ( ( unsigned int ) dimension_ > target_pos.size() )
    {
      throw KernelException(
        "Spatial distance dimension must be within the defined number of "
        "dimensions for the nodes." );
    }
    return std::abs( source_layer->compute_displacement( target_pos, source_lid )[ dimension_ - 1 ] );
  default:
    throw KernelException( String::compose(
      "SpatialDistanceParameter dimension must be either 0 for unspecified,"
      " or 1-3 for x-z. Got ",
      dimension_ ) );
    break;
  }
}

double
SpatialDistanceParameter::value( librandom::RngPtr& rng,
  const std::vector< double >& source_pos,
  const std::vector< double >& target_pos,
  const std::vector< double >& displacement ) const
{
  switch ( dimension_ )
  {
  case 0:
  {
    double sq_sum = 0;
    for ( auto&& disp_n : displacement )
    {
      sq_sum += disp_n * disp_n;
    }
    return std::sqrt( sq_sum );
  }
  case 1:
  case 2:
  case 3:
    if ( ( unsigned int ) dimension_ > displacement.size() )
    {
      throw KernelException(
        "Spatial distance dimension must be within the defined number of "
        "dimensions for the nodes." );
    }
    return std::abs( displacement[ dimension_ - 1 ] );
  default:
    throw KernelException( String::compose(
      "SpatialDistanceParameter dimension must be either 0 for unspecified,"
      " or 1-3 for x-z. Got ",
      dimension_ ) );
    break;
  }
}

} /* namespace nest */
