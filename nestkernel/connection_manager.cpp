/*
 *  connection_manager.cpp
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

#include "connection_manager.h"
#include "connector_base.h"
#include "network.h"
#include "spikecounter.h"
#include "nest_time.h"
#include "nest_datums.h"
#include "kernel_manager.h"

#include <algorithm>

#include <typeinfo>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace nest
{

ConnectionManager::ConnectionManager()
{
}

ConnectionManager::~ConnectionManager()
{
  delete_connections_();
}

void
ConnectionManager::init()
{
  init_();
}

void
ConnectionManager::init_()
{
  tVSConnector tmp( kernel().vp_manager.get_num_threads(), tSConnector() );
  connections_.swap( tmp );
  num_connections_ = 0;
}

void
ConnectionManager::delete_connections_()
{
  for ( tVSConnector::iterator it = connections_.begin(); it != connections_.end(); ++it )
    for ( tSConnector::nonempty_iterator iit = it->nonempty_begin(); iit != it->nonempty_end();
          ++iit )
#ifdef USE_PMA
      ( *iit )->~ConnectorBase();
#else
      delete ( *iit );
#endif

#if defined _OPENMP && defined USE_PMA
#ifdef IS_K
#pragma omp parallel
  {
    poormansallocpool[ omp_get_thread_num() ].destruct();
    poormansallocpool[ omp_get_thread_num() ].init();
  }
#else
#pragma omp parallel
  {
    poormansallocpool.destruct();
    poormansallocpool.init();
  }
#endif
#endif
}

void
ConnectionManager::reset()
{
  delete_connections_();
  init_();
}

const Time
ConnectionManager::get_min_delay() const
{
  Time min_delay = Time::pos_inf();

// TODO Re-factor similar to get_num_connections below.
//
//  std::vector< ConnectorModel* >::const_iterator it;
//  for ( thread t = 0; t < kernel().vp_manager.get_num_threads(); ++t )
//    for ( it = prototypes_[ t ].begin(); it != prototypes_[ t ].end(); ++it )
//      if ( *it != 0 && ( *it )->get_num_connections() > 0 )
//        min_delay = std::min( min_delay, ( *it )->get_min_delay() );

  return min_delay;
}

const Time
ConnectionManager::get_max_delay() const
{
  Time max_delay = Time::get_resolution();

// TODO Re-factor similar to get_num_connections below.
//
//  std::vector< ConnectorModel* >::const_iterator it;
//  for ( thread t = 0; t < kernel().vp_manager.get_num_threads(); ++t )
//    for ( it = prototypes_[ t ].begin(); it != prototypes_[ t ].end(); ++it )
//      if ( *it != 0 && ( *it )->get_num_connections() > 0 )
//        max_delay = std::max( max_delay, ( *it )->get_max_delay() );

  return max_delay;
}

bool
ConnectionManager::get_user_set_delay_extrema() const
{
  bool user_set_delay_extrema = false;

// TODO Re-factor similar to get_num_connections below.
//
//  std::vector< ConnectorModel* >::const_iterator it;
//  for ( thread t = 0; t < kernel().vp_manager.get_num_threads(); ++t )
//    for ( it = prototypes_[ t ].begin(); it != prototypes_[ t ].end(); ++it )
//      user_set_delay_extrema |= ( *it )->get_user_set_delay_extrema();

  return user_set_delay_extrema;
}

void
ConnectionManager::get_status( DictionaryDatum& d ) const
{
  size_t n = get_num_connections();
  def< long >( d, "num_connections", n );
}

DictionaryDatum
ConnectionManager::get_synapse_status( index gid, synindex syn_id, port p, thread tid )
{
  kernel().model_manager.assert_valid_syn_id( syn_id );

  DictionaryDatum dict( new Dictionary );
  connections_[ tid ].get( gid )->get_synapse_status( syn_id, dict, p );
  ( *dict )[ names::source ] = gid;
  Name name = kernel().model_manager.get_synapse_prototype( tid, syn_id ).get_name();
  ( *dict )[ names::synapse_model ] = LiteralDatum( name );

  return dict;
}

void
ConnectionManager::set_synapse_status( index gid,
  synindex syn_id,
  port p,
  thread tid,
  const DictionaryDatum& dict )
{
  kernel().model_manager.assert_valid_syn_id( syn_id );
  try
  {
    connections_[ tid ].get( gid )->set_synapse_status(
      syn_id, kernel().model_manager.get_synapse_prototype( tid, syn_id ), dict, p );
  }
  catch ( BadProperty& e )
  {
    throw BadProperty(
      String::compose( "Setting status of '%1' connecting from GID %2 to port %3: %4",
        kernel().model_manager.get_synapse_prototype( tid, syn_id ).get_name(),
        gid,
        p,
        e.message() ) );
  }
}


ArrayDatum
ConnectionManager::get_connections( DictionaryDatum params ) const
{
  ArrayDatum connectome;

  const Token& source_t = params->lookup( names::source );
  const Token& target_t = params->lookup( names::target );
  const Token& syn_model_t = params->lookup( names::synapse_model );
  const TokenArray* source_a = 0;
  const TokenArray* target_a = 0;

  if ( not source_t.empty() )
    source_a = dynamic_cast< TokenArray const* >( source_t.datum() );
  if ( not target_t.empty() )
    target_a = dynamic_cast< TokenArray const* >( target_t.datum() );

  size_t syn_id = 0;

#ifdef _OPENMP
  std::string msg;
  msg =
    String::compose( "Setting OpenMP num_threads to %1.", kernel().vp_manager.get_num_threads() );
  LOG( M_DEBUG, "ConnectionManager::get_connections", msg );
  omp_set_num_threads( kernel().vp_manager.get_num_threads() );
#endif

  // First we check, whether a synapse model is given.
  // If not, we will iterate all.
  if ( not syn_model_t.empty() )
  {
    Name synmodel_name = getValue< Name >( syn_model_t );
    const Token synmodel = kernel().model_manager.get_synapsedict()->lookup( synmodel_name );
    if ( !synmodel.empty() )
      syn_id = static_cast< size_t >( synmodel );
    else
      throw UnknownModelName( synmodel_name.toString() );
    get_connections( connectome, source_a, target_a, syn_id );
  }
  else
  {
    for ( syn_id = 0; syn_id < kernel().model_manager.get_num_synapse_prototypes(); ++syn_id )
    {
      ArrayDatum conn;
      get_connections( conn, source_a, target_a, syn_id );
      if ( conn.size() > 0 )
        connectome.push_back( new ArrayDatum( conn ) );
    }
  }

  return connectome;
}

void
ConnectionManager::get_connections( ArrayDatum& connectome,
  TokenArray const* source,
  TokenArray const* target,
  size_t syn_id ) const
{
  size_t num_connections = 0;

  // TODO Use ConnectionManager::get_num_connections( t, syn_id),
  // which has to be implemented. See TODO below.
  //for ( thread t = 0; t < kernel().vp_manager.get_num_threads(); ++t )
  //  num_connections += prototypes_[ t ][ syn_id ]->get_num_connections();

  connectome.reserve( num_connections );

  if ( source == 0 and target == 0 )
  {
#ifdef _OPENMP
#pragma omp parallel
    {
      thread t = kernel().vp_manager.get_thread_id();
#else
    for ( thread t = 0; t < Network::get_network().get_num_threads(); ++t )
    {
#endif
      ArrayDatum conns_in_thread;
      size_t num_connections_in_thread = 0;
      // Count how many connections we will have.
      for ( tSConnector::const_nonempty_iterator it = connections_[ t ].nonempty_begin();
            it != connections_[ t ].nonempty_end();
            ++it )
      {
        num_connections_in_thread += ( *it )->get_num_connections();
      }

#ifdef _OPENMP
#pragma omp critical( get_connections )
#endif
      conns_in_thread.reserve( num_connections_in_thread );
      for ( index source_id = 1; source_id < connections_[ t ].size(); ++source_id )
      {
        if ( connections_[ t ].get( source_id ) != 0 )
          connections_[ t ]
            .get( source_id )
            ->get_connections( source_id, t, syn_id, conns_in_thread );
      }
      if ( conns_in_thread.size() > 0 )
      {
#ifdef _OPENMP
#pragma omp critical( get_connections )
#endif
        connectome.append_move( conns_in_thread );
      }
    }

    return;
  }
  else if ( source == 0 and target != 0 )
  {
#ifdef _OPENMP
#pragma omp parallel
    {
      thread t = kernel().vp_manager.get_thread_id();
#else
    for ( thread t = 0; t < Network::get_network().get_num_threads(); ++t )
    {
#endif
      ArrayDatum conns_in_thread;
      size_t num_connections_in_thread = 0;
      // Count how many connections we will have maximally.
      for ( tSConnector::const_nonempty_iterator it = connections_[ t ].nonempty_begin();
            it != connections_[ t ].nonempty_end();
            ++it )
      {
        num_connections_in_thread += ( *it )->get_num_connections();
      }

#ifdef _OPENMP
#pragma omp critical( get_connections )
#endif
      conns_in_thread.reserve( num_connections_in_thread );
      for ( index source_id = 1; source_id < connections_[ t ].size(); ++source_id )
      {
        if ( connections_[ t ].get( source_id ) != 0 )
        {
          for ( index t_id = 0; t_id < target->size(); ++t_id )
          {
            size_t target_id = target->get( t_id );
            connections_[ t ]
              .get( source_id )
              ->get_connections( source_id, target_id, t, syn_id, conns_in_thread );
          }
        }
      }
      if ( conns_in_thread.size() > 0 )
      {
#ifdef _OPENMP
#pragma omp critical( get_connections )
#endif
        connectome.append_move( conns_in_thread );
      }
    }
    return;
  }
  else if ( source != 0 )
  {
#ifdef _OPENMP
#pragma omp parallel
    {
      size_t t = kernel().vp_manager.get_thread_id();
#else
    for ( thread t = 0; t < Network::get_network().get_num_threads(); ++t )
    {
#endif
      ArrayDatum conns_in_thread;
      size_t num_connections_in_thread = 0;
      // Count how many connections we will have maximally.
      for ( tSConnector::const_nonempty_iterator it = connections_[ t ].nonempty_begin();
            it != connections_[ t ].nonempty_end();
            ++it )
      {
        num_connections_in_thread += ( *it )->get_num_connections();
      }

#ifdef _OPENMP
#pragma omp critical( get_connections )
#endif
      conns_in_thread.reserve( num_connections_in_thread );
      for ( index s = 0; s < source->size(); ++s )
      {
        size_t source_id = source->get( s );
        if ( source_id < connections_[ t ].size() && connections_[ t ].get( source_id ) != 0 )
        {
          if ( target == 0 )
          {
            connections_[ t ]
              .get( source_id )
              ->get_connections( source_id, t, syn_id, conns_in_thread );
          }
          else
          {
            for ( index t_id = 0; t_id < target->size(); ++t_id )
            {
              size_t target_id = target->get( t_id );
              connections_[ t ]
                .get( source_id )
                ->get_connections( source_id, target_id, t, syn_id, conns_in_thread );
            }
          }
        }
      }

      if ( conns_in_thread.size() > 0 )
      {
#ifdef _OPENMP
#pragma omp critical( get_connections )
#endif
        connectome.append_move( conns_in_thread );
      }
    }
    return;
  } // else
}

ConnectorBase*
ConnectionManager::validate_source_entry( thread tid, index s_gid, synindex syn_id )
{
  kernel().model_manager.assert_valid_syn_id( syn_id );

  // resize sparsetable to full network size
  if ( connections_[ tid ].size() < Network::get_network().size() )
    connections_[ tid ].resize( Network::get_network().size() );

  // check, if entry exists
  // if not put in zero pointer
  if ( connections_[ tid ].test( s_gid ) )
    return connections_[ tid ].get(
      s_gid ); // returns non-const reference to stored type, here ConnectorBase*
  else
    return 0; // if non-existing
}


/*
   Connection::Manager::connect()

   Here a short description of the logic of the following connect() methods
   (from a mail conversation between HEP and MH, 2013-07-03)

   1. On the first line, conn is assigned from connections_[tid], may
      be 0.  It may be zero, if there is no outgoing connection from
      the neuron s_gid on this thread.  It will also create the sparse
      table for the specified thread tid, if it does not exist yet.

   2. After the second line, c will contain a pointer to a
      ConnectorBase object, c will never be zero. The pointer address
      conn may be changed by add_connection, due to suicide.
      This possibly new pointer is returned and stored in c.

   3. The third line inserts c into the same place where conn was
      taken from on the first line.  It stores the pointer conn in the
      sparse table, either overwriting the old value, if unequal 0, or
      creating a new entry.


   The parameters delay and weight have the default value NAN.
   NAN is a special value in cmath, which describes double values that
   are not a number. If delay or weight is omitted in an connect call,
   NAN indicates this and weight/delay are set only, if they are valid.
*/

void
ConnectionManager::connect( Node& s,
  Node& r,
  index s_gid,
  thread tid,
  index syn,
  double_t d,
  double_t w )
{
  // see comment above for explanation
  ConnectorBase* conn = validate_source_entry( tid, s_gid, syn );
  ConnectorBase* c = kernel().model_manager.get_synapse_prototype( tid, syn ).add_connection( s, r, conn, syn, d, w );
  connections_[ tid ].set( s_gid, c );
}

void
ConnectionManager::connect( Node& s,
  Node& r,
  index s_gid,
  thread tid,
  index syn,
  DictionaryDatum& p,
  double_t d,
  double_t w )
{
  // see comment above for explanation
  ConnectorBase* conn = validate_source_entry( tid, s_gid, syn );
  ConnectorBase* c = kernel().model_manager.get_synapse_prototype( tid, syn ).add_connection( s, r, conn, syn, p, d, w );
  connections_[ tid ].set( s_gid, c );
}

/**
 * Connect, using a dictionary with arrays.
 * This variant of connect combines the functionalities of
 * - connect
 * - divergent_connect
 * - convergent_connect
 * The decision is based on the details of the dictionary entries source and target.
 * If source and target are both either a GID or a list of GIDs with equal size, then source and
 * target are connected one-to-one.
 * If source is a gid and target is a list of GIDs then divergent_connect is used.
 * If source is a list of GIDs and target is a GID, then convergent_connect is used.
 * At this stage, the task of connect is to separate the dictionary into one for each thread and
 * then to forward the
 * connect call to the connectors who can then deal with the details of the connection.
 */
bool
ConnectionManager::connect( ArrayDatum& conns )
{
  // #ifdef _OPENMP
  //     LOG(M_INFO, "ConnectionManager::Connect", msg);
  // #endif

  // #ifdef _OPENMP
  // #pragma omp parallel shared

  // #endif
  {
    for ( Token* ct = conns.begin(); ct != conns.end(); ++ct )
    {
      DictionaryDatum cd = getValue< DictionaryDatum >( *ct );
      index target_gid = static_cast< size_t >( ( *cd )[ names::target ] );
      Node* target_node = Network::get_network().get_node( target_gid );
      size_t thr = target_node->get_thread();

      // #ifdef _OPENMP
      // 	    size_t my_thr=omp_get_thread_num();
      // 	    if(my_thr == thr)
      // #endif
      {

        size_t syn_id = 0;
        index source_gid = ( *cd )[ names::source ];

        Token synmodel = cd->lookup( names::synapse_model );
        if ( !synmodel.empty() )
        {
          std::string synmodel_name = getValue< std::string >( synmodel );
          synmodel = kernel().model_manager.get_synapsedict()->lookup( synmodel_name );
          if ( !synmodel.empty() )
            syn_id = static_cast< size_t >( synmodel );
          else
            throw UnknownModelName( synmodel_name );
        }
        Node* source_node = Network::get_network().get_node( source_gid );
        //#pragma omp critical
        connect( *source_node, *target_node, source_gid, thr, syn_id, cd );
      }
    }
  }
  return true;
}

void
ConnectionManager::trigger_update_weight( const long_t vt_id,
  const vector< spikecounter >& dopa_spikes,
  const double_t t_trig )
{
  for ( index t = 0; t < kernel().vp_manager.get_num_threads(); ++t )
    for ( tSConnector::const_nonempty_iterator it = connections_[ t ].nonempty_begin();
          it != connections_[ t ].nonempty_end();
          ++it )
      ( *it )->trigger_update_weight( vt_id, t, dopa_spikes, t_trig, kernel().model_manager.get_prototypes( t ) );
}

void
ConnectionManager::send( thread t, index sgid, Event& e )
{
  if ( sgid < connections_[ t ].size() ) // probably test only fails, if there are no connections
    if ( connections_[ t ].get( sgid ) != 0 ) // only send, if connections exist
      connections_[ t ].get( sgid )->send( e, t, kernel().model_manager.get_prototypes( t ) );
}

size_t
ConnectionManager::get_num_connections() const
{
  num_connections_ = 0;

// TODO is not moved to ModelManager as this function should be part
// of the ConnectionManager. The iteration of 'prototypes' needs to be
// replaced by an iteration of a new 2-dim (thread, syn_id)
// datastructure to store the number of connections in the
// ConnectionManager itself. The variable num_connections of
// ConnectorModel needs to be removed and its increment in
// connector_model_impl.h:431 needs to be replaced by a function call
// to ConnectionManager::increment_num_connections(syn_id).
//
//  for ( thread t = 0; t < kernel().vp_manager.get_num_threads(); ++t )
//    for ( std::vector< ConnectorModel* >::const_iterator i = prototypes_[ t ].begin();
//          i != prototypes_[ t ].end();
//          ++i )
//      num_connections_ += ( *i )->get_num_connections();

  return num_connections_;
}

} // namespace
