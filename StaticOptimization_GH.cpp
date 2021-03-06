/* -------------------------------------------------------------------------- *
 *                      OpenSim:  StaticOptimization_GH.cpp                      *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2012 Stanford University and the Authors                *
 * Author(s): Jeffrey A. Reinbolt                                             *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */


//=============================================================================
// INCLUDES
//=============================================================================
#include <iostream>
#include <string>
#include <OpenSim/Common/IO.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/SimbodyEngine/SimbodyEngine.h>
#include <OpenSim/Simulation/Model/BodySet.h>
#include <OpenSim/Simulation/Model/CoordinateSet.h>
#include <OpenSim/Simulation/Model/ForceSet.h>
#include <OpenSim/Simulation/Model/Muscle.h>
#include <OpenSim/Common/GCVSplineSet.h>
#include <OpenSim/Actuators/CoordinateActuator.h>
#include <OpenSim/Simulation/Control/ControlSet.h>
#include <SimTKmath.h>
#include <SimTKlapack.h>
#include "StaticOptimization_GH.h"
#include "StaticOptimization_GHTarget.h"
#include <OpenSim/Simulation/Model/ActivationFiberLengthMuscle.h>


using namespace OpenSim;
using namespace std;

//=============================================================================
// CONSTRUCTOR(S) AND DESTRUCTOR
//=============================================================================
//_____________________________________________________________________________
/**
 * Destructor.
 */
StaticOptimization_GH::~StaticOptimization_GH()
{
	deleteStorage();
	delete _modelWorkingCopy;
	if(_ownsForceSet) delete _forceSet;
    delete _forceReporter;
}
//_____________________________________________________________________________
/**
 */
StaticOptimization_GH::StaticOptimization_GH(Model *aModel) :
    Analysis(aModel),
	_useModelForceSet(_useModelForceSetProp.getValueBool()),
	_activationExponent(_activationExponentProp.getValueDbl()),
	_useMusclePhysiology(_useMusclePhysiologyProp.getValueBool()),
	_GHstabTransitionParam(_GHstabTransitionParamProp.getValueDbl()),
	_atheta(_athetaProp.getValueDbl()),
	_aphi(_aphiProp.getValueDbl()),
	_convergenceCriterion(_convergenceCriterionProp.getValueDbl()),
	_maximumIterations(_maximumIterationsProp.getValueInt()),
	_modelWorkingCopy(NULL),
	_numCoordinateActuators(0),
    _forceReporter(0)
{
	setNull();

	if(aModel) setModel(*aModel);
	else allocateStorage();
}
// Copy constrctor and virtual copy 
//_____________________________________________________________________________
/**
 * Copy constructor.
 *
 */
StaticOptimization_GH::StaticOptimization_GH(const StaticOptimization_GH &aStaticOptimization_GH):
	Analysis(aStaticOptimization_GH),
	_useModelForceSet(_useModelForceSetProp.getValueBool()),
	_activationExponent(_activationExponentProp.getValueDbl()),
	_useMusclePhysiology(_useMusclePhysiologyProp.getValueBool()),
	_GHstabTransitionParam(_GHstabTransitionParamProp.getValueDbl()),
	_atheta(_athetaProp.getValueDbl()),
	_aphi(_aphiProp.getValueDbl()),
	_convergenceCriterion(_convergenceCriterionProp.getValueDbl()),
	_maximumIterations(_maximumIterationsProp.getValueInt()),
	_modelWorkingCopy(NULL),
	_numCoordinateActuators(aStaticOptimization_GH._numCoordinateActuators)
{
	setNull();
	// COPY TYPE AND NAME
	*this = aStaticOptimization_GH;
    _forceReporter = 0;
}

//=============================================================================
// OPERATORS
//=============================================================================
//-----------------------------------------------------------------------------
// ASSIGNMENT
//-----------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Assign this object to the values of another.
 *
 * @return Reference to this object.
 */
StaticOptimization_GH& StaticOptimization_GH::
operator=(const StaticOptimization_GH &aStaticOptimization_GH)
{
	// BASE CLASS
	Analysis::operator=(aStaticOptimization_GH);

	_modelWorkingCopy = aStaticOptimization_GH._modelWorkingCopy;
	_numCoordinateActuators = aStaticOptimization_GH._numCoordinateActuators;
	_useModelForceSet = aStaticOptimization_GH._useModelForceSet;
	_activationExponent=aStaticOptimization_GH._activationExponent;
	_convergenceCriterion=aStaticOptimization_GH._convergenceCriterion;
	_maximumIterations=aStaticOptimization_GH._maximumIterations;
	_GHstabTransitionParam = aStaticOptimization_GH._GHstabTransitionParam;
	_atheta = aStaticOptimization_GH._atheta;
	_aphi = aStaticOptimization_GH._aphi;

	_useMusclePhysiology=aStaticOptimization_GH._useMusclePhysiology;
    _forceReporter = aStaticOptimization_GH._forceReporter;
	return(*this);
}

//_____________________________________________________________________________
/**
 * SetNull().
 */
void StaticOptimization_GH::setNull()
{
	setAuthors("Dimitra Blana");
	setupProperties();

	// OTHER VARIABLES
	_useModelForceSet = true;
	_activationStorage = NULL;
	_ownsForceSet = false;
	_forceSet = NULL;
	_activationExponent=2;
	_useMusclePhysiology=true;
	_numCoordinateActuators = 0;
	_GHstabTransitionParam = 0.01;
	_atheta = 0.7744;
	_aphi = 0.6728;
	_convergenceCriterion = 1e-4;
	_maximumIterations = 100;
    _forceReporter = 0;

	setName("StaticOptimization_GH");
}
//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 */
void StaticOptimization_GH::
setupProperties()
{
	_useModelForceSetProp.setComment("If true, the model's own force set will be used in the static optimization computation.  "
													"Otherwise, inverse dynamics for coordinate actuators will be computed for all unconstrained degrees of freedom.");
	_useModelForceSetProp.setName("use_model_force_set");
	_propertySet.append(&_useModelForceSetProp);

	_activationExponentProp.setComment(
		"A double indicating the exponent to raise activations to when solving static optimization.  ");
	_activationExponentProp.setName("activation_exponent");
	_propertySet.append(&_activationExponentProp);

	
	_useMusclePhysiologyProp.setComment(
		"If true muscle force-length curve is observed while running optimization.");
	_useMusclePhysiologyProp.setName("use_muscle_physiology");
	_propertySet.append(&_useMusclePhysiologyProp);

	_GHstabTransitionParamProp.setComment(
		"Transition parameter used to determine the shape of the GH stability curve in the cost function");
	_GHstabTransitionParamProp.setName("GH_stability_transition_parameter");
	_propertySet.append(&_GHstabTransitionParamProp);

	_athetaProp.setName("theta_at_glenoid_rim");
	_athetaProp.setComment("Angle along the major axis of the ellipse representing the glenoid fossa.");
	_propertySet.append(&_athetaProp);

	_aphiProp.setName("phi_at_glenoid_rim");
	_aphiProp.setComment("Angle along the minor axis of the ellipse representing the glenoid fossa.");
	_propertySet.append(&_aphiProp);

	_convergenceCriterionProp.setComment(
		"Value used to determine when the optimization solution has converged");
	_convergenceCriterionProp.setName("optimizer_convergence_criterion");
	_propertySet.append(&_convergenceCriterionProp);

	_maximumIterationsProp.setComment(
		"An integer for setting the maximum number of iterations the optimizer can use at each time.  ");
	_maximumIterationsProp.setName("optimizer_max_iterations");
	_propertySet.append(&_maximumIterationsProp);
}

//=============================================================================
// CONSTRUCTION METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Construct a description for the static optimization files.
 */
void StaticOptimization_GH::
constructDescription()
{
	string descrip = "This file contains static optimization results.\n\n";
	setDescription(descrip);
}

//_____________________________________________________________________________
/**
 * Construct column labels for the static optimization files.
 */
void StaticOptimization_GH::
constructColumnLabels()
{
	Array<string> labels;
	labels.append("time");
	if(_model) 
        for (int i = 0; i < _forceSet->getActuators().getSize(); i++) labels.append(_forceSet->getActuators().get(i).getName());
	setColumnLabels(labels);
}

//_____________________________________________________________________________
/**
 * Allocate storage for the static optimization.
 */
void StaticOptimization_GH::
allocateStorage()
{
	_activationStorage = new Storage(1000,"Static Optimization GH");
	_activationStorage->setDescription(getDescription());
	_activationStorage->setColumnLabels(getColumnLabels());
}


//=============================================================================
// DESTRUCTION METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Delete storage objects.
 */
void StaticOptimization_GH::
deleteStorage()
{
	delete _activationStorage; _activationStorage = NULL;
}

//=============================================================================
// GET AND SET
//=============================================================================
//_____________________________________________________________________________
/**
 * Set the model for which the static optimization is to be computed.
 *
 * @param aModel Model pointer
 */
void StaticOptimization_GH::
setModel(Model& aModel)
{
	Analysis::setModel(aModel);
}

//-----------------------------------------------------------------------------
// STORAGE
//-----------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Get the activation storage.
 *
 * @return Activation storage.
 */
Storage* StaticOptimization_GH::
getActivationStorage()
{
	return(_activationStorage);
}
//_____________________________________________________________________________
/**
 * Get the force storage.
 *
 * @return Force storage.
 */
Storage* StaticOptimization_GH::
getForceStorage()
{
    if (_forceReporter)
        return(&_forceReporter->updForceStorage());
    else
        return NULL;
}

//-----------------------------------------------------------------------------
// STORAGE CAPACITY
//-----------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Set the capacity increments of all storage instances.
 *
 * @param aIncrement Increment by which storage capacities will be increased
 * when storage capacities run out.
 */
void StaticOptimization_GH::
setStorageCapacityIncrements(int aIncrement)
{
	_activationStorage->setCapacityIncrement(aIncrement);
}

//=============================================================================
// ANALYSIS
//=============================================================================
//_____________________________________________________________________________
/**
 * Record the results.
 */
int StaticOptimization_GH::
record(const SimTK::State& s)
{
	if(!_modelWorkingCopy) return -1;

	// Set model to whatever defaults have been updated to from the last iteration
    SimTK::State& sWorkingCopy = _modelWorkingCopy->updWorkingState();
	sWorkingCopy.setTime(s.getTime());
	_modelWorkingCopy->initStateWithoutRecreatingSystem(sWorkingCopy); 

	// update Q's and U's
	sWorkingCopy.setQ(s.getQ());
	sWorkingCopy.setU(s.getU());

	_modelWorkingCopy->getMultibodySystem().realize(sWorkingCopy, SimTK::Stage::Velocity);
	//_modelWorkingCopy->equilibrateMuscles(sWorkingCopy);

    const Set<Actuator>& acts = _modelWorkingCopy->getActuators();

    int na = acts.getSize();
	int nacc = _accelerationIndices.getSize();

	// IPOPT
	_numericalDerivativeStepSize = 0.0001;
	_optimizerAlgorithm = "ipopt";
	_printLevel = 0;
	//_optimizationConvergenceTolerance = 1e-004;
	//_maxIterations = 2000;

	// Optimization target
	_modelWorkingCopy->setAllControllersEnabled(false);
	StaticOptimization_GHTarget target(sWorkingCopy,_modelWorkingCopy,na,nacc,_useMusclePhysiology);
	target.setStatesStore(_statesStore);
	target.setStatesSplineSet(_statesSplineSet);
	target.setActivationExponent(_activationExponent);
	target.setGHstabTransitionParam(_GHstabTransitionParam);
	target.setatheta(_atheta);
	target.setaphi(_aphi);
	target.setDX(_numericalDerivativeStepSize);

	// Pick optimizer algorithm
	SimTK::OptimizerAlgorithm algorithm = SimTK::InteriorPoint;
	//SimTK::OptimizerAlgorithm algorithm = SimTK::CFSQP;

	// Optimizer
	SimTK::Optimizer *optimizer = new SimTK::Optimizer(target, algorithm);

	// Optimizer options
	//cout<<"\nSetting optimizer print level to "<<_printLevel<<".\n";
	optimizer->setDiagnosticsLevel(_printLevel);
	//cout<<"Setting optimizer convergence criterion to "<<_convergenceCriterion<<".\n";
	optimizer->setConvergenceTolerance(_convergenceCriterion);
	//cout<<"Setting optimizer maximum iterations to "<<_maximumIterations<<".\n";
	optimizer->setMaxIterations(_maximumIterations);
	optimizer->useNumericalGradient(true);
	optimizer->useNumericalJacobian(false);
	if(algorithm == SimTK::InteriorPoint) {
		// Some IPOPT-specific settings
		optimizer->setLimitedMemoryHistory(500); // works well for our small systems
		optimizer->setAdvancedBoolOption("warm_start",true);
		optimizer->setAdvancedRealOption("obj_scaling_factor",1);
		optimizer->setAdvancedRealOption("nlp_scaling_max_gradient",1);
	}

	// Parameter bounds
	SimTK::Vector lowerBounds(na), upperBounds(na);
	for(int i=0,j=0;i<acts.getSize();i++) {
		Actuator& act = acts.get(i);
		lowerBounds(j) = act.getMinControl();
	    upperBounds(j) = act.getMaxControl();
        j++;
	}
	
	target.setParameterLimits(lowerBounds, upperBounds);

	_parameters = 0; // Set initial guess to zeros

	// Static optimization
	_modelWorkingCopy->getMultibodySystem().realize(sWorkingCopy,SimTK::Stage::Velocity);
	target.prepareToOptimize(sWorkingCopy, &_parameters[0]);

	//LARGE_INTEGER start;
	//LARGE_INTEGER stop;
	//LARGE_INTEGER frequency;

	//QueryPerformanceFrequency(&frequency);
	//QueryPerformanceCounter(&start);

	try {
		target.setCurrentState( &sWorkingCopy );
		optimizer->optimize(_parameters);
	}
	catch (const SimTK::Exception::Base& ex) {
		cout << ex.getMessage() << endl;
		cout << "OPTIMIZATION FAILED..." << endl;
		cout << endl;
		cout << "StaticOptimization_GH.record:  WARN- The optimizer could not find a solution at time = " << s.getTime() << endl;
		cout << endl;

		double tolBounds = 1e-1;
		bool weakModel = false;
		string msgWeak = "The model appears too weak for static optimization.\nTry increasing the strength and/or range of the following force(s):\n";
		for(int a=0;a<na;a++) {
			Actuator* act = dynamic_cast<Actuator*>(&_forceSet->get(a));
            if( act ) {
			    Muscle*  mus = dynamic_cast<Muscle*>(&_forceSet->get(a));
 			    if(mus==NULL) {
			    	if(_parameters(a) < (lowerBounds(a)+tolBounds)) {
			    		msgWeak += "   ";
			    		msgWeak += act->getName();
			    		msgWeak += " approaching lower bound of ";
			    		ostringstream oLower;
			    		oLower << lowerBounds(a);
			    		msgWeak += oLower.str();
			    		msgWeak += "\n";
			    		weakModel = true;
			    	} else if(_parameters(a) > (upperBounds(a)-tolBounds)) {
			    		msgWeak += "   ";
			    		msgWeak += act->getName();
			    		msgWeak += " approaching upper bound of ";
			    		ostringstream oUpper;
			    		oUpper << upperBounds(a);
			    		msgWeak += oUpper.str();
			    		msgWeak += "\n";
			    		weakModel = true;
			    	} 
			    } else {
			    	if(_parameters(a) > (upperBounds(a)-tolBounds)) {
			    		msgWeak += "   ";
			    		msgWeak += mus->getName();
			    		msgWeak += " approaching upper bound of ";
			    		ostringstream o;
			    		o << upperBounds(a);
			    		msgWeak += o.str();
			    		msgWeak += "\n";
			    		weakModel = true;
			    	}
			    }
            }
		}
		if(weakModel) cout << msgWeak << endl;

		if(!weakModel) {
			double tolConstraints = 1e-6;
			bool incompleteModel = false;
			string msgIncomplete = "The model appears unsuitable for static optimization.\nTry appending the model with additional force(s) or locking joint(s) to reduce the following acceleration constraint violation(s):\n";
			SimTK::Vector constraints;
			target.constraintFunc(_parameters,true,constraints);
			const CoordinateSet& coordSet = _modelWorkingCopy->getCoordinateSet();
			for(int acc=0;acc<nacc;acc++) {
				if(fabs(constraints(acc)) > tolConstraints) {
					const Coordinate& coord = coordSet.get(_accelerationIndices[acc]);
					msgIncomplete += "   ";
					msgIncomplete += coord.getName();
					msgIncomplete += ": constraint violation = ";
					ostringstream o;
					o << constraints(acc);
					msgIncomplete += o.str();
					msgIncomplete += "\n";
					incompleteModel = true;
				}
			}
            _forceReporter->step(sWorkingCopy, 1);
			if(incompleteModel) cout << msgIncomplete << endl;
		}
	}

	//QueryPerformanceCounter(&stop);
	//double duration = (double)(stop.QuadPart-start.QuadPart)/(double)frequency.QuadPart;
	//cout << "optimizer time = " << (duration*1.0e3) << " milliseconds" << endl;

	target.printPerformance(sWorkingCopy, &_parameters[0]);

	//update defaults for use in the next step

	const Set<Actuator>& actuators = _modelWorkingCopy->getActuators();
	for(int k=0; k < actuators.getSize(); ++k){
		ActivationFiberLengthMuscle *mus = dynamic_cast<ActivationFiberLengthMuscle*>(&actuators[k]);
		if(mus){
			mus->setDefaultActivation(_parameters[k]);
			// Don't send up red flags when the def
			mus->setObjectIsUpToDateWithProperties();
		}
	}

	_activationStorage->append(sWorkingCopy.getTime(),na,&_parameters[0]);

	SimTK::Vector forces(na);
	target.getActuation(const_cast<SimTK::State&>(sWorkingCopy), _parameters,forces);

    _forceReporter->step(sWorkingCopy, 1);
	return 0;
}
//_____________________________________________________________________________
/**
 * This method is called at the beginning of an analysis so that any
 * necessary initializations may be performed.
 *
 * This method is meant to be called at the begining of an integration 
 *
 * @param s Current state .
 *
 * @return -1 on error, 0 otherwise.
 */
int StaticOptimization_GH::
begin(SimTK::State& s )
{
	if(!proceed()) return(0);

	// Make a working copy of the model
	delete _modelWorkingCopy;
	_modelWorkingCopy = _model->clone();
	_modelWorkingCopy->initSystem();

	// Replace model force set with only generalized forces
	if(_model) {
        SimTK::State& sWorkingCopyTemp = _modelWorkingCopy->updWorkingState();
		// Update the _forceSet we'll be computing inverse dynamics for
		if(_ownsForceSet) delete _forceSet;
		if(_useModelForceSet) {
			// Set pointer to model's internal force set
			_forceSet = &_modelWorkingCopy->updForceSet();
			_ownsForceSet = false;

		} else {
			ForceSet& as = _modelWorkingCopy->updForceSet();
			// Keep a copy of forces that are not muscles to restore them back.
			ForceSet* saveForces = as.clone();
			// Generate an force set consisting of a coordinate actuator for every unconstrained degree of freedom
			_forceSet = CoordinateActuator::CreateForceSetOfCoordinateActuatorsForModel(sWorkingCopyTemp,*_modelWorkingCopy,1,false);
			_ownsForceSet = false;
			_modelWorkingCopy->setAllControllersEnabled(false);
			_numCoordinateActuators = _forceSet->getSize();
			// Copy whatever forces that are not muscles back into the model
			
			for(int i=0; i<saveForces->getSize(); i++){
				const Force& f=saveForces->get(i);
				if ((dynamic_cast<const Muscle*>(&saveForces->get(i)))==NULL)
					as.append(saveForces->get(i).clone());
			}
		}

		SimTK::State& sWorkingCopy = _modelWorkingCopy->initSystem();

		// Set modeiling options for Actuators to be overriden
		for(int i=0; i<_forceSet->getSize(); i++) {
			Actuator* act = dynamic_cast<Actuator*>(&_forceSet->get(i));
			if( act ) {
				act->overrideForce(sWorkingCopy,true);
			}
		}
        sWorkingCopy.setTime(s.getTime()); //Keep time in sync.
		sWorkingCopy.setQ(s.getQ());
		sWorkingCopy.setU(s.getU());
		sWorkingCopy.setZ(s.getZ());
		_modelWorkingCopy->getMultibodySystem().realize(s,SimTK::Stage::Velocity);
		_modelWorkingCopy->equilibrateMuscles(sWorkingCopy);
		// Gather indices into speed set corresponding to the unconstrained degrees of freedom (for which we will set acceleration constraints)
		_accelerationIndices.setSize(0);
		const CoordinateSet& coordSet = _model->getCoordinateSet();
		for(int i=0; i<coordSet.getSize(); i++) {
			const Coordinate& coord = coordSet.get(i);
			if(!coord.isConstrained(sWorkingCopy)) {
				_accelerationIndices.append(i);
			}
		}

		int nf = _forceSet->getSize();
		int nacc = _accelerationIndices.getSize();

		if(nf < nacc) 
			throw(Exception("StaticOptimization_GH: ERROR- overconstrained system -- need at least as many forces as there are degrees of freedom.\n"));
        
        // initialize forceReporter using modified set of Forces and state from corresponding model
        delete _forceReporter;
        _forceReporter = new ForceReporter(_modelWorkingCopy);
        _forceReporter->begin(sWorkingCopy);
        _forceReporter->updForceStorage().reset();
		_parameters.resize(_forceSet->getActuators().getSize());
		_parameters = 0;
	}

	_statesSplineSet=GCVSplineSet(5,_statesStore);

	// DESCRIPTION AND LABELS
	constructDescription();
	constructColumnLabels();

	deleteStorage();
	allocateStorage();

	// RESET STORAGE
	_activationStorage->reset(s.getTime());

	// RECORD
	int status = 0;
	if(_activationStorage->getSize()<=0) {
		status = record(s);
		const Set<Actuator>& fs = _modelWorkingCopy->getActuators();
		for(int k=0;k<fs.getSize();k++) {
			Actuator& act = fs.get(k);
			cout << "Bounds for " << act.getName() << ": " << act.getMinControl()<< " to "<< act.getMaxControl() << endl;
		}
	}

	return(status);
}
//_____________________________________________________________________________
/**
 * This method is called to perform the analysis.  It can be called during
 * the execution of a forward integrations or after the integration by
 * feeding it the necessary data.
 *
 * This method should be overriden in derived classes.  It is
 * included here so that the derived class will not have to implement it if
 * it is not necessary.
 *
 * @param s Current state .
 *
 * @return -1 on error, 0 otherwise.
 */
int StaticOptimization_GH::
step(const SimTK::State& s, int stepNumber )
{
	if(!proceed(stepNumber)) return(0);

	record(s);

	return(0);
}
//_____________________________________________________________________________
/**
 * This method is called at the end of an analysis so that any
 * necessary finalizations may be performed.
 *
 * @param s Current state 
 *
 * @return -1 on error, 0 otherwise.
 */
int StaticOptimization_GH::
end( SimTK::State& s )
{
	if(!proceed()) return(0);

	record(s);

	return(0);
}


//=============================================================================
// IO
//=============================================================================
//_____________________________________________________________________________
/**
 * Print results.
 * 
 * The file names are constructed as
 * aDir + "/" + aBaseName + "_" + ComponentName + aExtension
 *
 * @param aDir Directory in which the results reside.
 * @param aBaseName Base file name.
 * @param aDT Desired time interval between adjacent storage vectors.  Linear
 * interpolation is used to print the data out at the desired interval.
 * @param aExtension File extension.
 *
 * @return 0 on success, -1 on error.
 */
int StaticOptimization_GH::
printResults(const string &aBaseName,const string &aDir,double aDT,
				 const string &aExtension)
{
	// ACTIVATIONS
	Storage::printResult(_activationStorage,aBaseName+"_"+getName()+"_activation",aDir,aDT,aExtension);

	// FORCES
	Storage::printResult(getForceStorage(),aBaseName+"_"+getName()+"_force",aDir,aDT,aExtension);

	// Make a ControlSet out of activations for use in forward dynamics
	ControlSet cs(*_activationStorage);
	std::string path = (aDir=="") ? "." : aDir;
	std::string name = path + "/" + aBaseName+"_"+getName()+"_controls.xml";
	cs.print(name);
	return(0);
}
