// Copyright (c) 2012 CNRS
// Author: Florent Lamiraux
//
// This file is part of hpp-manipulation-corba.
// hpp-manipulation-corba is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// hpp-manipulation-corba is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Lesser Public License for more details.  You should have
// received a copy of the GNU Lesser General Public License along with
// hpp-manipulation-corba.  If not, see
// <http://www.gnu.org/licenses/>.

#include <hpp/corbaserver/server.hh>
#include <hpp/corbaserver/wholebody-step/server.hh>
#include <hpp/corbaserver/manipulation/server.hh>
#include <hpp/manipulation/problem-solver.hh>

typedef hpp::corbaServer::Server CorbaServer;

typedef hpp::wholebodyStep::Server WholebodyServer;

typedef hpp::manipulation::Server ManipulationServer;
typedef hpp::manipulation::ProblemSolver ProblemSolver;
typedef hpp::manipulation::ProblemSolverPtr_t ProblemSolverPtr_t;
typedef hpp::manipulation::ManipulationPlanner ManipulationPlanner;
typedef hpp::manipulation::ManipulationPlannerPtr_t ManipulationPlannerPtr_t;

int main (int argc, char* argv [])
{
  ProblemSolverPtr_t problemSolver = new ProblemSolver();

  CorbaServer corbaServer (problemSolver, argc,
			   const_cast<const char**> (argv), true);
  WholebodyServer wbsServer (argc, const_cast<const char**> (argv), true);
  wbsServer.setProblemSolverMap (corbaServer.problemSolverMap());
  ManipulationServer manipServer (argc, const_cast<const char**> (argv), true);
  manipServer.setProblemSolverMap (corbaServer.problemSolverMap());

  corbaServer.startCorbaServer ();
  wbsServer.startCorbaServer ("hpp", "corbaserver",
				"wholebodyStep", "problem");
  manipServer.startCorbaServer ("hpp", "corbaserver",
				"manipulation");
  corbaServer.processRequest(true);
}
