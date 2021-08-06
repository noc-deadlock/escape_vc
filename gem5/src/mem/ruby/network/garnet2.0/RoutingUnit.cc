/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Niket Agarwal
 *          Tushar Krishna
 */


#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"

#include "base/cast.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/slicc_interface/Message.hh"

using namespace std;

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vcs_per_vnet = m_router->get_vc_per_vnet();
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(const NetDest& routing_table_entry)
{
    m_routing_table.push_back(routing_table_entry);
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */

int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {

            if (m_weight_table[link] == min_weight) {

                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int vc, int inport,
                            PortDirection inport_dirn)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    // override here based on the vc.
    // this override the choice of routing algorithm as well
    int vc_base = route.vnet*m_vcs_per_vnet;
    if (routing_algorithm == ESCAPE_VC_RANDOM_) {
        if (vc == vc_base)
            routing_algorithm = ADAPT_WF_;
        else
            routing_algorithm = RANDOM_;
    } else if (routing_algorithm == ESCAPE_VC_ADAPT_RANDOM_) {
        if (vc == vc_base)
            routing_algorithm = ADAPT_WF_;
        else
            routing_algorithm = ADAPT_RANDOM_;
    } 

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        case TURN_MODEL_: outport =
            outportComputeTurnModel(route, inport, inport_dirn); break;
        case ADAPT_RANDOM_: outport =
            outportComputeAdaptRandom(route, inport, inport_dirn); break;
        case RANDOM_: outport =
            outportComputeRandom(route, inport, inport_dirn); break;
        case ADAPT_WF_: outport =
            outportComputeAdaptWF(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    int M5_VAR_USED num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        assert(0);
    }

    return m_outports_dirn2idx[outport_dirn];
}


int
RoutingUnit::outportComputeTurnModel(RouteInfo route,
                                    int inport,
                                    PortDirection inport_dirn)
{

    PortDirection outport_dirn = "Unknown";

    int M5_VAR_USED num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    /////////////////////////////////////////
    // ICN Lab 3: Insert code here
    // mparasar: Implementing west-first
    if (x_hops == 0)
    {
        if (y_dirn > 0)
            outport_dirn = "North";
        else
            outport_dirn = "South";
    }
    else if (y_hops == 0)
    {
        if (x_dirn > 0)
            outport_dirn = "East";
        else
            outport_dirn = "West";
    } else {
        int rand = random() % 2;

        if (x_dirn && y_dirn) // Quadrant I
            outport_dirn = rand ? "East" : "North";
        else if (!x_dirn && y_dirn) // Quadrant II
            outport_dirn = "West";
        else if (!x_dirn && !y_dirn) // Quadrant III
            outport_dirn = "West";
        else // Quadrant IV
            outport_dirn = rand ? "East" : "South";
    }

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeAdaptWF(RouteInfo route,
                        int inport,
                        PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    int M5_VAR_USED num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    /////////////////////////////////////////
    // ICN Lab 3: Insert code here
    // mparasar: Implementing west-first
    if (x_hops == 0)
    {
        if (y_dirn > 0)
            outport_dirn = "North";
        else
            outport_dirn = "South";
    }
    else if (y_hops == 0)
    {
        if (x_dirn > 0)
            outport_dirn = "East";
        else
            outport_dirn = "West";
    } else {
        int rand = random() % 2;

        // instead of choosing randomly
        // choose the direction which has more
        // credits present in the outVCState.
        if (x_dirn && y_dirn) {
            // Quadrant I
            int credit_est = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_est += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["East"]]->get_credit_count(vc_);

            }
            // cout << "credit_est: " << credit_est << endl;
            int credit_nrth = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_nrth += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["North"]]->get_credit_count(vc_);

            }
            // cout << "credit_nrth: " << credit_nrth << endl;
            if (credit_est > credit_nrth)
                outport_dirn = "East";
            else if (credit_nrth > credit_est)
                outport_dirn = "North";
            else
                outport_dirn = rand ? "East" : "North";
        }
        else if (!x_dirn && y_dirn) {
            // Quadrant II
            outport_dirn = "West";
        }
        else if (!x_dirn && !y_dirn) {
            // Quadrant III
            outport_dirn = "West";
        }
        else {
            // Quadrant IV
            int credit_est = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_est += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["East"]]->get_credit_count(vc_);

            }
            // cout << "credit_est: " << credit_est << endl;
            int credit_south = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_south += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["North"]]->get_credit_count(vc_);

            }
            // cout << "credit_south: " << credit_south << endl;
            if (credit_est > credit_south)
                outport_dirn = "East";
            else if (credit_south > credit_est)
                outport_dirn = "South";
            else
                outport_dirn = rand ? "East" : "South";
        }
    }

    return m_outports_dirn2idx[outport_dirn];


}


int
RoutingUnit::outportComputeAdaptRandom(RouteInfo route,
                        int inport,
                        PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    int M5_VAR_USED num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    /////////////////////////////////////////
    // ICN Lab 3: Insert code here
    if (x_hops == 0)
    {
        if (y_dirn > 0)
            outport_dirn = "North";
        else
            outport_dirn = "South";
    }
    else if (y_hops == 0)
    {
        if (x_dirn > 0)
            outport_dirn = "East";
        else
            outport_dirn = "West";
    } else {
        int rand = random() % 2;

        // instead of choosing randomly
        // choose the direction which has more
        // credits present in the outVCState.
        if (x_dirn && y_dirn) {
            // Quadrant I
            int credit_est = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_est += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["East"]]->get_credit_count(vc_);

            }
            // cout << "credit_est: " << credit_est << endl;
            int credit_nrth = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_nrth += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["North"]]->get_credit_count(vc_);

            }
            // cout << "credit_nrth: " << credit_nrth << endl;
            if (credit_est > credit_nrth)
                outport_dirn = "East";
            else if (credit_nrth > credit_est)
                outport_dirn = "North";
            else
                outport_dirn = rand ? "East" : "North";
        }
        else if (!x_dirn && y_dirn) {
            // Quadrant II
            int credit_wst = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_wst += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["West"]]->get_credit_count(vc_);

            }
            // cout << "credit_wst: " << credit_wst << endl;
            int credit_nrth = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_nrth += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["North"]]->get_credit_count(vc_);

            }
            // cout << "credit_nrth: " << credit_nrth << endl;
            if (credit_wst > credit_nrth)
                outport_dirn = "West";
            else if (credit_nrth > credit_wst)
                outport_dirn = "North";
            else
                outport_dirn = rand ? "West" : "North";
        }
        else if (!x_dirn && !y_dirn) {
            // Quadrant III
            int credit_wst = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_wst += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["West"]]->get_credit_count(vc_);

            }
            // cout << "credit_wst: " << credit_wst << endl;
            int credit_south = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_south += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["South"]]->get_credit_count(vc_);

            }
            // cout << "credit_south: " << credit_south << endl;
            if (credit_wst > credit_south)
                outport_dirn = "West";
            else if (credit_south > credit_wst)
                outport_dirn = "South";
            else
                outport_dirn = rand ? "West" : "South";
        }
        else {
            // Quadrant IV
            int credit_est = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_est += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["East"]]->get_credit_count(vc_);

            }
            // cout << "credit_est: " << credit_est << endl;
            int credit_south = 0;
            for (int vc_ = 0; vc_ < m_vcs_per_vnet; vc_++) {
             credit_south += m_router->\
                get_outputUnit_ref()[m_outports_dirn2idx["North"]]->get_credit_count(vc_);

            }
            // cout << "credit_south: " << credit_south << endl;
            if (credit_est > credit_south)
                outport_dirn = "East";
            else if (credit_south > credit_est)
                outport_dirn = "South";
            else
                outport_dirn = rand ? "East" : "South";
        }
    }

    return m_outports_dirn2idx[outport_dirn];


}



int
RoutingUnit::outportComputeRandom(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    int M5_VAR_USED num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops == 0)
    {
        if (y_dirn > 0)
            outport_dirn = "North";
        else
            outport_dirn = "South";
    }
    else if (y_hops == 0)
    {
        if (x_dirn > 0)
            outport_dirn = "East";
        else
            outport_dirn = "West";
    } else {
        int rand = random() % 2;

        if (x_dirn && y_dirn) // Quadrant I
            outport_dirn = rand ? "East" : "North";
        else if (!x_dirn && y_dirn) // Quadrant II
            outport_dirn = rand ? "West" : "North";
        else if (!x_dirn && !y_dirn) // Quadrant III
            outport_dirn = rand ? "West" : "South";
        else // Quadrant IV
            outport_dirn = rand ? "East" : "South";
    }

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    assert(0);
    return -1;
}