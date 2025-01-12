//
//  main.cpp
//  g2o_tutorial_slam2d
//
//  Tested by Seung-Chan in OSX (10.10) on 1/1/17.
//  Original code : https://github.com/RainerKuemmerle/g2o/tree/master/g2o/examples/tutorial_slam2d


#include <iostream>
#include <cmath>

#include "simulator.h"

#include "vertex_se2.h"
#include "vertex_point_xy.h"
#include "edge_se2.h"
#include "edge_se2_pointxy.h"
#include "types_tutorial_slam2d.h"

#include "g2o/core/sparse_optimizer.h"
#include "g2o/core/block_solver.h"
#include "g2o/core/factory.h"
#include "g2o/core/optimization_algorithm_factory.h"
#include "g2o/core/optimization_algorithm_gauss_newton.h"
#include "g2o/solvers/csparse/linear_solver_csparse.h"

using namespace std;
using namespace g2o;
using namespace g2o::tutorial;

int main()
{
    // TODO simulate different sensor offset
    // simulate a robot observing landmarks while travelling on a grid
    SE2 sensorOffsetTransf(0.2, 0.1, -0.1); // SE2(double x, double y, double theta):_R(theta),_t(x,y){}
    // p.truePose * sensorOffset
    
    cout << "SensorOffset : " << endl;
    cout << " translation : \n" << sensorOffsetTransf.translation().transpose() << endl;
    cout << " rotation : \n" << sensorOffsetTransf.rotation().toRotationMatrix() << endl;
    
    int numNodes = 300;
    
    //
    Simulator simulator;
    simulator.simulate(numNodes, sensorOffsetTransf);
    
    /*********************************************************************************
     * creating the optimization problem
     ********************************************************************************/
    
    typedef BlockSolver< BlockSolverTraits<-1, -1> >  SlamBlockSolver;
    typedef LinearSolverCSparse<SlamBlockSolver::PoseMatrixType> SlamLinearSolver;
    
    // allocating the optimizer
    SparseOptimizer optimizer;
    
    SlamLinearSolver* linearSolver = new SlamLinearSolver();
    linearSolver->setBlockOrdering(false);
    SlamBlockSolver* blockSolver = new SlamBlockSolver(linearSolver);
    OptimizationAlgorithmGaussNewton* solver = new OptimizationAlgorithmGaussNewton(blockSolver);
    
    optimizer.setAlgorithm(solver);
    
    // add the parameter representing the sensor offset
    // note that class ParameterSE2Offset is inhereted from Parameter class
    
    ParameterSE2Offset* sensorOffset = new ParameterSE2Offset;
    sensorOffset->setOffset(sensorOffsetTransf);
    sensorOffset->setId(0);
    optimizer.addParameter(sensorOffset);
    
    // adding the odometry to the optimizer
    // first adding all the vertices
    cerr << "Optimization: Adding robot poses ... ";
    //ofstream fs;
    //fs.open("../../../out/out_poses_before.txt");
    for (size_t i = 0; i < simulator.poses().size(); ++i)
    {
        const Simulator::GridPose& p = simulator.poses()[i];
        const SE2& t = p.simulatorPose;
        VertexSE2* robot =  new VertexSE2;
        robot->setId(p.id);
        robot->setEstimate(t);
        //cout << p.id << " " << t.translation().x() << " " << t.translation().y() << endl;
        //fs << p.id << " " << t.translation().x() << " " << t.translation().y() << endl;
        
        optimizer.addVertex(robot);
        
    }
    //fs.close();
    
    
    
    
    
        
    cerr << "done." << endl;
    
    // second add the odometry constraints
    cerr << "Optimization: Adding odometry measurements ... ";
    for (size_t i = 0; i < simulator.odometry().size(); ++i)
    {
        const Simulator::GridEdge& simEdge = simulator.odometry()[i];
        
        EdgeSE2* odometry = new EdgeSE2;
        odometry->vertices()[0] = optimizer.vertex(simEdge.from);
        odometry->vertices()[1] = optimizer.vertex(simEdge.to);
        odometry->setMeasurement(simEdge.simulatorTransf);
        odometry->setInformation(simEdge.information);
        optimizer.addEdge(odometry);
    }
    cerr << "done." << endl;
    
    // add the landmark observations
    cerr << "Optimization: add landmark vertices ... ";
    for (size_t i = 0; i < simulator.landmarks().size(); ++i)
    {
        const Simulator::Landmark& l = simulator.landmarks()[i];
        VertexPointXY* landmark = new VertexPointXY;
        landmark->setId(l.id);
        landmark->setEstimate(l.simulatedPose); // simulatedPose <-- Eigen::Vector2d;
        optimizer.addVertex(landmark);
    }
    cerr << "done." << endl;
    
    cerr << "Optimization: add landmark observations ... ";
    for (size_t i = 0; i < simulator.landmarkObservations().size(); ++i)
    {
        const Simulator::LandmarkEdge& simEdge = simulator.landmarkObservations()[i];
        EdgeSE2PointXY* landmarkObservation =  new EdgeSE2PointXY;
        landmarkObservation->vertices()[0] = optimizer.vertex(simEdge.from);    //p.id;
        landmarkObservation->vertices()[1] = optimizer.vertex(simEdge.to);      //l->id;
        landmarkObservation->setMeasurement(simEdge.simulatorMeas);             //observation : (p.simulatorPose * sensorOffset).inverse() * l->simulatedPose;
        landmarkObservation->setInformation(simEdge.information);
        landmarkObservation->setParameterId(0, sensorOffset->id());
        optimizer.addEdge(landmarkObservation);
    }
    cerr << "done." << endl;
    
    //
    ofstream fs;
    fs.open("../../../out/out_measurements.txt");

    for (size_t i = 0; i < simulator.landmarkObservations().size(); ++i)
    {
        const Simulator::LandmarkEdge& simEdge = simulator.landmarkObservations()[i];
        EdgeSE2PointXY* landmarkObservation =  new EdgeSE2PointXY;
        EdgeSE2PointXY v1;
        //v1.vertices()[0]=optimizer.vertex(simEdge.from);
        Eigen::Vector2d simulatorMeas = simEdge.simulatorMeas;
        
        fs << i << "\t" << simulatorMeas.x() << "\t" << simulatorMeas.y() << "\t" <<endl;
        
        //landmarkObservation->vertices()[1] = optimizer.vertex(simEdge.to);      //l->id;
    }
    fs.close();
    
    //optimizer.vertices();
    
    fs.open("../../../out/out_simEdge_from_before.txt");
    for (size_t i = 0; i < simulator.landmarkObservations().size(); ++i)
    {
     
        const Simulator::LandmarkEdge& simEdge = simulator.landmarkObservations()[i];
        
        optimizer.vertex(simEdge.from);
        
        VertexSE2* v = dynamic_cast<VertexSE2*> (optimizer.vertex(simEdge.from));
        //cout << i << "\t" << v->estimate().translation().transpose() << endl;;
        fs << i << "\t" << v->estimate().translation().transpose() << endl;;
        //Eigen::Isometry2d t = v->estimate();
        
    }
    fs.close();
    
    
    
    /*********************************************************************************
     * optimization
     ********************************************************************************/
    
    // dump initial state to the disk
    optimizer.save("tutorial_before.g2o");
    
    // prepare and run the optimization
    // fix the first robot pose to account for gauge freedom
    VertexSE2* firstRobotPose = dynamic_cast<VertexSE2*>(optimizer.vertex(0));
    firstRobotPose->setFixed(true);
    optimizer.setVerbose(true);
    
    cerr << "Optimizing" << endl;
    optimizer.initializeOptimization();
    optimizer.optimize(10);
    cerr << "done." << endl;
    
    fs.open("../../../out/out_simEdge_from_after.txt");
    for (size_t i = 0; i < simulator.landmarkObservations().size(); ++i)
    {
        
        const Simulator::LandmarkEdge& simEdge = simulator.landmarkObservations()[i];
        
        optimizer.vertex(simEdge.from);
        
        VertexSE2* v = dynamic_cast<VertexSE2*> (optimizer.vertex(simEdge.from));
        //cout << i << "\t" << v->estimate().translation().transpose() << endl;;
        fs << i << "\t" << v->estimate().translation().transpose() << endl;;
        //Eigen::Isometry2d t = v->estimate();
        
    }
    fs.close();
    
    optimizer.save("tutorial_after.g2o");
    
    
    // freeing the graph memory
    optimizer.clear();
    
    
    // destroy all the singletons
    Factory::destroy();
    OptimizationAlgorithmFactory::destroy();
    return 0;
    
    HyperGraphActionLibrary::destroy(); // crashes : pointer being freed was not allocated !!
    
    return 0;
}
