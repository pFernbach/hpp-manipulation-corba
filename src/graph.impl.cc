// Copyright (c) 2014, LAAS-CNRS
// Authors: Joseph Mirabel (joseph.mirabel@laas.fr)
//
// This file is part of hpp-manipulation.
// hpp-manipulation is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// hpp-manipulation is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Lesser Public License for more details.  You should have
// received a copy of the GNU Lesser General Public License along with
// hpp-manipulation. If not, see <http://www.gnu.org/licenses/>.

#include <fstream>
#include <sstream>

#include <hpp/util/debug.hh>
#include <hpp/util/pointer.hh>

#include <hpp/manipulation/problem.hh>
#include <hpp/manipulation/roadmap.hh>
#include <hpp/manipulation/graph/node-selector.hh>
#include <hpp/manipulation/graph/guided-node-selector.hh>
#include <hpp/manipulation/graph/node.hh>
#include <hpp/manipulation/graph/graph.hh>
#include <hpp/manipulation/graph/edge.hh>
#include <hpp/manipulation/constraint-set.hh>

#include "graph.impl.hh"

namespace hpp {
  namespace manipulation {
    namespace impl {
      using CORBA::ULong;

      namespace {
        template <typename T> std::string toStr () { return typeid(T).name(); }
        template <> std::string toStr <graph::Node> () { return "Node"; }
        template <> std::string toStr <graph::Edge> () { return "Edge"; }
        template <> std::string toStr <graph::Graph> () { return "Graph"; }
        template <> std::string toStr <graph::NodeSelector> () { return "SubGraph"; }
        template <> std::string toStr <graph::GuidedNodeSelector> () { return "SubGraph"; }
        template <> std::string toStr <graph::LevelSetEdge> () { return "LevelSetEdge"; }
        template <> std::string toStr <graph::WaypointEdge> () { return "WaypointEdge"; }

        template <typename T> boost::shared_ptr<T> getComp (ID id, bool throwIfWrongType = true) { 
          boost::shared_ptr <T> comp;
          try {
            comp = HPP_DYNAMIC_PTR_CAST(T,
                graph::GraphComponent::get(id).lock());
          } catch (std::out_of_range& e) {
            throw Error (e.what());
          }
          if (throwIfWrongType && !comp) {
            std::stringstream ss;
            ss << "ID " << id << " is not a " << toStr <T>();
            throw Error (ss.str().c_str());
          }
          return comp;
        }
      }

      std::vector <std::string> expandPassiveDofsNameVector (
          const hpp::Names_t& names, const size_t& s)
      {
        assert (s >= names.length ());
        std::vector <std::string> ret (s, std::string ());
        for (CORBA::ULong i=0; i<names.length (); ++i)
          ret [i] = std::string (names[i]);
        return ret;
      }

      Graph::Graph () :
        problemSolver_ (0x0), graph_ ()
      {}

      Long Graph::createGraph(const char* graphName)
        throw (hpp::Error)
      {
        DevicePtr_t robot = problemSolver_->robot ();
        if (!robot) throw Error ("Build the robot first.");
	// Create default steering method to store in edges, until we define a
	// factory for steering methods.
        graph_ = graph::Graph::create(graphName, robot,
            problemSolver_->problem());
        graph_->maxIterations (problemSolver_->maxIterations ());
        graph_->errorThreshold (problemSolver_->errorThreshold ());
        problemSolver_->constraintGraph (graph_);
        problemSolver_->problem()->constraintGraph (graph_);
        return (Long) graph_->id ();
      }

      Long Graph::createSubGraph(const char* subgraphName)
        throw (hpp::Error)
      {
        if (!graph_)
          throw Error ("You should create the graph"
              " before creating subgraph.");
        graph::GuidedNodeSelectorPtr_t ns = graph::GuidedNodeSelector::create
          (subgraphName, problemSolver_->roadmap ());
        graph_->nodeSelector(ns);
        return (Long) ns->id ();
      }

      void Graph::setTargetNodeList(const ID subgraph, const hpp::IDseq& nodes)
        throw (hpp::Error)
      {
        graph::GuidedNodeSelectorPtr_t ns = getComp <graph::GuidedNodeSelector> (subgraph);
        try {
          graph::Nodes_t nl;
          for (unsigned int i = 0; i < nodes.length(); ++i)
            nl.push_back (getComp <graph::Node> (nodes[i]));
          ns->setNodeList (nl);
        } catch (std::out_of_range& e) {
          throw Error (e.what());
        }
      }

      Long Graph::createNode(const Long subgraphId, const char* nodeName)
        throw (hpp::Error)
      {
        graph::NodeSelectorPtr_t ns = getComp <graph::NodeSelector> (subgraphId);

        graph::NodePtr_t node = ns->createNode (nodeName);
        return (Long) node->id ();
      }

      Long Graph::createEdge(const Long nodeFromId, const Long nodeToId, const char* edgeName, const Long w, const bool isInNodeFrom)
        throw (hpp::Error)
      {
        graph::NodePtr_t from = getComp <graph::Node> (nodeFromId),
                         to   = getComp <graph::Node> (nodeToId  );

        graph::EdgePtr_t edge = from->linkTo (edgeName, to, w, isInNodeFrom);
        return (Long) edge->id ();
      }

      void Graph::createWaypointEdge(const Long nodeFromId, const Long nodeToId,
          const char* edgeBaseName, const Long nb, const Long w, const bool isInNodeFrom, GraphElements_out out_elmts)
        throw (hpp::Error)
      {
        graph::NodePtr_t from = getComp <graph::Node> (nodeFromId),
                         to   = getComp <graph::Node> (nodeToId  );

        std::ostringstream ss; ss << edgeBaseName << "_e" << nb;
        graph::EdgePtr_t edge_pc = from->linkTo (ss.str (), to, w, isInNodeFrom,
						 graph::WaypointEdge::create);
        graph::WaypointEdgePtr_t edge = HPP_DYNAMIC_PTR_CAST (graph::WaypointEdge, edge_pc);
        edge->createWaypoint (nb - 1, edgeBaseName);
        std::list <graph::EdgePtr_t> edges;
        graph::WaypointEdgePtr_t cur = edge;
        while (cur->waypoint <graph::WaypointEdge> ()) {
          cur = cur->waypoint <graph::WaypointEdge> ();
          edges.push_front (cur);
        }
        edges.push_front (cur->waypoint <graph::Edge> ());

        GraphComps_t n, e;
        GraphComp gc;
        e.length ((ULong) edges.size () + 1);
        n.length ((ULong) edges.size ());
        size_t r = 0;
        for (std::list <graph::EdgePtr_t>::const_iterator it = edges.begin ();
            it != edges.end (); it++) {
          gc.name = (*it)->name ().c_str ();
          gc.id = (Long) (*it)->id ();
          e[(ULong) r] = gc;
          gc.name = (*it)->to ()->name ().c_str ();
          gc.id = (Long) (*it)->to ()->id ();
          n[(ULong) r] = gc;
          r++;
        }
        gc.name = edge->name ().c_str ();
        gc.id = (Long) edge->id ();
        e[(ULong) r] = gc;
        out_elmts = new GraphElements;
        out_elmts->nodes = n;
        out_elmts->edges = e;
      }

      void Graph::getGraph (GraphComp_out graph_out, GraphElements_out elmts)
        throw (hpp::Error)
      {
        if (!graph_) throw Error ("There is no graph");
        GraphComps_t comp_n, comp_e;
        GraphComp comp_g, current;

        graph::NodePtr_t n;
        graph::EdgePtr_t e;

        CORBA::ULong len_edges = 0;
        CORBA::ULong len_nodes = 0;
        try {
          // Set the graph values
          graph_out = new GraphComp ();
          graph_out->name = graph_->name ().c_str();
          graph_out->id = (Long) graph_->id ();

          for (std::size_t i = 0;
	       i < graph::GraphComponent::components().size (); ++i) {
            if (i == graph_->id ()) continue;
            graph::GraphComponentPtr_t gcomponent = graph::GraphComponent::get(i).lock();
            if (!gcomponent) continue;
            current.name = gcomponent->name ().c_str ();
            current.id   = (Long) gcomponent->id ();
            n = HPP_DYNAMIC_PTR_CAST(graph::Node, gcomponent);
            e = HPP_DYNAMIC_PTR_CAST(graph::Edge, gcomponent);
            if (n) {
              comp_n.length (len_nodes + 1);
              comp_n[len_nodes] = current;
              len_nodes++;
            } else if (e) {
              comp_e.length (len_edges + 1);
              graph::WaypointEdgePtr_t we = HPP_DYNAMIC_PTR_CAST (
                  graph::WaypointEdge, e);
              if (we) {
                current.start = (Long)we->waypoint<graph::Edge>()->to ()->id ();
                current.end = (Long) e->to ()->id ();
              } else {
                current.start = (Long) e->from ()->id ();
                current.end = (Long) e->to ()->id ();
              }
              comp_e[len_edges] = current;
              len_edges++;
            }
          }
          elmts = new GraphElements;
          elmts->nodes = comp_n;
          elmts->edges = comp_e;
        } catch (std::out_of_range& e) {
          throw Error (e.what());
        }
      }

      bool Graph::getConfigProjectorStats (ID elmt, ConfigProjStat_out config,
          ConfigProjStat_out path)
        throw (hpp::Error)
      {
        if (!graph_)
          throw Error ("You should create the graph");
        graph::NodePtr_t node = getComp <graph::Node> (elmt, false);
        graph::EdgePtr_t edge = getComp <graph::Edge> (elmt, false);
        if (node) {
          ConfigProjectorPtr_t proj =
            graph_->configConstraint (node)->configProjector ();
          if (proj) {
            config.success = (Long) proj->statistics().nbSuccess();
            config.error = (Long) proj->statistics().nbFailure();
            config.nbObs = (Long) proj->statistics().numberOfObservations();
          }
          path.success = 0;
          path.error = 0;
          path.nbObs = 0;
          return true;
        } else if (edge) {
          ConfigProjectorPtr_t proj =
            graph_->configConstraint (edge)->configProjector ();
          if (proj) {
            config.success = (Long) proj->statistics().nbSuccess();
            config.error = (Long) proj->statistics().nbFailure();
            config.nbObs = (Long) proj->statistics().numberOfObservations();
          }
          proj = graph_->pathConstraint (edge)->configProjector ();
          if (proj) {
            path.success = (Long) proj->statistics().nbSuccess();
            path.error = (Long) proj->statistics().nbFailure();
            path.nbObs = (Long) proj->statistics().numberOfObservations();
          }
          return true;
        } else {
          throw Error ("The ID does not exist.");
        }
        return false;
      }

      Long Graph::getWaypoint (const Long edgeId, hpp::ID_out nodeId)
        throw (hpp::Error)
      {
        graph::WaypointEdgePtr_t edge = getComp <graph::WaypointEdge> (edgeId);

        graph::EdgePtr_t waypoint = edge->waypoint <graph::Edge> ();
        nodeId = (Long) waypoint->to ()->id ();
        return (Long) waypoint->id ();
      }

      Long Graph::createLevelSetEdge(const Long nodeFromId, const Long nodeToId, const char* edgeName, const Long w, const bool isInNodeFrom)
        throw (hpp::Error)
      {
        graph::NodePtr_t from = getComp <graph::Node> (nodeFromId),
                         to   = getComp <graph::Node> (nodeToId  );

        graph::EdgePtr_t edge = from->linkTo (edgeName, to, w, isInNodeFrom,
					      graph::LevelSetEdge::create);
        return (Long) edge->id ();
      }

      void Graph::setLevelSetFoliation (const Long edgeId,
          const hpp::Names_t& condNC, const hpp::Names_t& condLJ,
          const hpp::Names_t& paramNC, const hpp::Names_t& paramPDOF,
          const hpp::Names_t& paramLJ)
        throw (hpp::Error)
      {
        graph::LevelSetEdgePtr_t edge = getComp <graph::LevelSetEdge> (edgeId);
        try {
          for (CORBA::ULong i=0; i<condNC.length (); ++i) {
            std::string name (condNC [i]);
            edge->insertConditionConstraint (
                HPP_STATIC_PTR_CAST(NumericalConstraint,
                problemSolver_->get <NumericalConstraintPtr_t>(name)->copy ())
                );
          }
          for (CORBA::ULong i=0; i<condLJ.length (); ++i) {
            std::string name (condLJ [i]);
            edge->insertConditionConstraint (problemSolver_->get <LockedJointPtr_t> (name));
          }

          std::vector <std::string> pdofNames = expandPassiveDofsNameVector
            (paramPDOF, paramNC.length ());
          for (CORBA::ULong i=0; i<paramNC.length (); ++i) {
            std::string name (paramNC [i]);
            edge->insertParamConstraint (
                HPP_STATIC_PTR_CAST(NumericalConstraint,
                problemSolver_->get <NumericalConstraintPtr_t>(name)->copy ()),
                problemSolver_->passiveDofs (pdofNames [i]));
          }
          for (CORBA::ULong i=0; i<paramLJ.length (); ++i) {
            std::string name (paramLJ [i]);
            edge->insertParamConstraint (problemSolver_->get <LockedJointPtr_t> (name));
          }

          edge->buildHistogram ();
          RoadmapPtr_t roadmap = HPP_DYNAMIC_PTR_CAST (Roadmap, problemSolver_->roadmap());
          if (!roadmap)
            throw Error ("The roadmap is not of type hpp::manipulation::Roadmap.");
          roadmap->insertHistogram (edge->histogram ());
        } catch (std::exception& err) {
          throw Error (err.what());
        }
      }

      void Graph::isInNodeFrom (const Long edgeId, const bool isInNodeFrom)
        throw (hpp::Error)
      {
        graph::EdgePtr_t edge = getComp <graph::Edge> (edgeId);
        try {
          edge->isInNodeFrom (isInNodeFrom);
        } catch (std::exception& err) {
          throw Error (err.what());
        }
      }

      void Graph::setNumericalConstraints (const Long graphComponentId,
          const hpp::Names_t& constraintNames,
          const hpp::Names_t& passiveDofsNames)
        throw (hpp::Error)
      {
        graph::GraphComponentPtr_t component = graph::GraphComponent::get(graphComponentId).lock();
        if (!component)
          throw Error ("The ID does not exist.");

        if (constraintNames.length () > 0) {
          try {
            std::vector <std::string> pdofNames = expandPassiveDofsNameVector
              (passiveDofsNames, constraintNames.length ());
            for (CORBA::ULong i=0; i<constraintNames.length (); ++i) {
              std::string name (constraintNames [i]);
              if (!problemSolver_->numericalConstraint (name))
                throw Error ("The numerical function does not exist.");
              component->addNumericalConstraint
		(HPP_STATIC_PTR_CAST
		 (NumericalConstraint,
		  problemSolver_->numericalConstraint(name)->copy ()),
		 problemSolver_->passiveDofs (pdofNames [i]));
            }
          } catch (std::exception& err) {
            throw Error (err.what());
          }
        }
      }

      void Graph::setNumericalConstraintsForPath (const Long nodeId,
          const hpp::Names_t& constraintNames,
          const hpp::Names_t& passiveDofsNames)
        throw (hpp::Error)
      {
        graph::NodePtr_t n = getComp <graph::Node> (nodeId);

        if (constraintNames.length () > 0) {
          try {
            std::vector <std::string> pdofNames = expandPassiveDofsNameVector
              (passiveDofsNames, constraintNames.length ());
            for (CORBA::ULong i=0; i<constraintNames.length (); ++i) {
              std::string name (constraintNames [i]);
              n->addNumericalConstraintForPath
		(HPP_STATIC_PTR_CAST
		 (NumericalConstraint,
		  problemSolver_->numericalConstraint(name)->copy ()),
		 problemSolver_->passiveDofs (pdofNames [i]));
            }
          } catch (std::exception& err) {
            throw Error (err.what());
          }
        }
      }

      void Graph::setLockedDofConstraints (const Long graphComponentId,
          const hpp::Names_t& constraintNames)
        throw (hpp::Error)
      {
        graph::GraphComponentPtr_t component = graph::GraphComponent::get(graphComponentId).lock();
        if (!component)
          throw Error ("The ID does not exist.");

        if (constraintNames.length () > 0) {
          try {
            for (CORBA::ULong i=0; i<constraintNames.length (); ++i) {
              std::string name (constraintNames [i]);
              component->addLockedJointConstraint
		(problemSolver_->get <LockedJointPtr_t> (name));
            }
          } catch (std::exception& err) {
            throw Error (err.what());
          }
        }
      }

      void Graph::getNode (const hpp::floatSeq& dofArray, ID_out output)
        throw (hpp::Error)
      {
        try {
          vector_t config; config.resize (dofArray.length());
          for (int iDof = 0; iDof < config.size(); iDof++) {
            config [iDof] = dofArray[iDof];
          }
          graph::NodePtr_t node = graph_->getNode (config);
          output = (Long) node->id();
        } catch (std::exception& e) {
          throw Error (e.what());
        }
      }

      CORBA::Boolean Graph::getConfigErrorForNode
      (const hpp::floatSeq& dofArray, ID nodeId, hpp::floatSeq_out error)
	throw (hpp::Error)
      {
	graph::NodePtr_t node = getComp <graph::Node> (nodeId);
	try {
	  vector_t err;
          Configuration_t config; config.resize (dofArray.length());
          for (std::size_t iDof = 0; iDof < (std::size_t)config.size();
	       ++iDof) {
            config [iDof] = dofArray[(ULong) iDof];
          }
	  bool res = graph_->getConfigErrorForNode (config, node, err);
	  floatSeq* e = new floatSeq ();
	  e->length ((ULong) err.size ());
	  for (std::size_t i=0; i < (std::size_t) err.size (); ++i) {
	    (*e) [(ULong) i] = err [i];
	  }
	  error = e;
	  return res;
	} catch (const std::exception& exc) {
	  throw Error (exc.what ());
	}
      }

      void Graph::displayNodeConstraints
      (hpp::ID nodeId, CORBA::String_out constraints) throw (Error)
      {
	graph::NodePtr_t node = getComp <graph::Node> (nodeId);
	ConstraintSetPtr_t cs (graph_->configConstraint (node));
	std::ostringstream oss;
	oss << (*cs);
	constraints = oss.str ().c_str ();
      }

      void Graph::displayEdgeConstraints
      (hpp::ID edgeId, CORBA::String_out constraints) throw (Error)
      {
	graph::EdgePtr_t edge = getComp <graph::Edge> (edgeId);
	ConstraintSetPtr_t cs (graph_->configConstraint (edge));
	std::ostringstream oss;
	oss << (*cs);
	constraints = oss.str ().c_str ();
      }

      void Graph::display (const char* filename)
        throw (hpp::Error)
      {
        std::cout << *graph_;
        std::ofstream dotfile;
        dotfile.open (filename);
        graph_->dotPrint (dotfile);
        dotfile.close();
      }

      void Graph::getHistogramValue (ID edgeId, hpp::floatSeq_out freq,
          hpp::floatSeqSeq_out values)
        throw (hpp::Error)
      {
        graph::LevelSetEdgePtr_t edge = getComp <graph::LevelSetEdge> (edgeId);
        try {
          graph::LeafHistogramPtr_t hist = edge->histogram ();
	  floatSeq* _freq = new floatSeq ();
          floatSeqSeq *_values = new floatSeqSeq ();
          _freq->length (hist->numberOfBins ());
          _values->length (hist->numberOfBins ());
          size_type i = 0;
          for (graph::LeafHistogram::const_iterator it = hist->begin ();
              it != hist->end (); ++it) {
            (*_freq)[i] = it->freq ();
            floatSeq v;
            const vector_t& offset = it->value();
            v.length (offset.size());
            for (size_type j = 0; j < offset.size(); ++j)
              v[j] = offset [j];
            (*_values)[i] = v;
            i++;
          }
          freq = _freq;
          values = _values;
	} catch (const std::exception& exc) {
	  throw Error (exc.what ());
	}
      }

      void Graph::setShort (ID edgeId, CORBA::Boolean isShort)
        throw (hpp::Error)
      {
        graph::EdgePtr_t edge = getComp <graph::Edge> (edgeId);
        try {
          edge->setShort (isShort);
	} catch (const std::exception& exc) {
	  throw Error (exc.what ());
	}
      }
    } // namespace impl
  } // namespace manipulation
} // namespace hpp
