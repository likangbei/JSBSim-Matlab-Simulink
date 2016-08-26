/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGFDMExec.cpp
 Author:       Jon S. Berndt
 Date started: 11/17/98
 Purpose:      Schedules and runs the model routines.

 ------------- Copyright (C) 1999  Jon S. Berndt (jsb@hal-pc.org) -------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 details.

 You should have received a copy of the GNU Lesser General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU Lesser General Public License can also be found on
 the world wide web at http://www.gnu.org.

FUNCTIONAL DESCRIPTION
--------------------------------------------------------------------------------

This class wraps up the simulation scheduling routines.

HISTORY
--------------------------------------------------------------------------------
11/17/98   JSB   Created

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
COMMENTS, REFERENCES,  and NOTES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "FGFDMExec.h"
#include "FGState.h"
#include <models/FGAtmosphere.h>
#include <models/atmosphere/FGMSIS.h>
#include <models/atmosphere/FGMars.h>
#include <models/FGFCS.h>
#include <models/FGPropulsion.h>
#include <models/FGMassBalance.h>
#include <models/FGGroundReactions.h>
#include <models/FGExternalReactions.h>
#include <models/FGBuoyantForces.h>
#include <models/FGAerodynamics.h>
#include <models/FGInertial.h>
#include <models/FGAircraft.h>
#include <models/FGPropagate.h>
#include <models/FGAuxiliary.h>
#include <models/FGInput.h>
#include <models/FGOutput.h>
#include <initialization/FGInitialCondition.h>
//#include <initialization/FGTrimAnalysis.h> // Remove until later
#include <input_output/FGPropertyManager.h>
#include <input_output/FGScript.h>

#include <iostream>
#include <iterator>

namespace JSBSim {

static const char *IdSrc = "$Id: FGFDMExec.cpp,v 1.56 2008/12/23 14:28:50 jberndt Exp $";
static const char *IdHdr = ID_FDMEXEC;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
GLOBAL DECLARATIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

unsigned int FGFDMExec::FDMctr = 0;
FGPropertyManager* FGFDMExec::master=0;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

void checkTied ( FGPropertyManager *node )
{
  int N = node->nChildren();
  string name;

  for (int i=0; i<N; i++) {
    if (node->getChild(i)->nChildren() ) {
//      cout << "Untieing " << node->getChild(i)->getName() << " property branch." << endl;
      checkTied( (FGPropertyManager*)node->getChild(i) );
    } else if ( node->getChild(i)->isTied() ) {
      name = ((FGPropertyManager*)node->getChild(i))->GetFullyQualifiedName();
      node->Untie(name);
    }
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Constructor

FGFDMExec::FGFDMExec(FGPropertyManager* root) : Root(root)
{

  Frame           = 0;
  FirstModel      = 0;
  Error           = 0;
  GroundCallback  = 0;
  State           = 0;
  Atmosphere      = 0;
  FCS             = 0;
  Propulsion      = 0;
  MassBalance     = 0;
  Aerodynamics    = 0;
  Inertial        = 0;
  GroundReactions = 0;
  ExternalReactions = 0;
  BuoyantForces   = 0;
  Aircraft        = 0;
  Propagate       = 0;
  Auxiliary       = 0;
  Input           = 0;
  IC              = 0;
  Trim            = 0;
  Script          = 0;

  modelLoaded = false;
  IsSlave = false;
  holding = false;
  Terminate = false;

  // Multiple FDM's are stopped for now.  We need to ensure that
  // the "user" instance always gets the zeroeth instance number,
  // because there may be instruments or scripts tied to properties
  // in the jsbsim[0] node.
  // ToDo: it could be that when JSBSim is reset and a new FDM is wanted, that
  // process might try setting FDMctr = 0. Then the line below would not need
  // to be commented out.
  IdFDM = FDMctr;
  //FDMctr++;

  try {
    char* num = getenv("JSBSIM_DEBUG");
    if (num) debug_lvl = atoi(num); // set debug level
  } catch (...) {               // if error set to 1
    debug_lvl = 1;
  }

  if (Root == 0) {
    if (master == 0)
      master = new FGPropertyManager;
    Root = master;
  }

  instance = Root->GetNode("/fdm/jsbsim",IdFDM,true);
  Debug(0);
  // this is to catch errors in binding member functions to the property tree.
  try {
    Allocate();
  } catch ( string msg ) {
    cout << "Caught error: " << msg << endl;
    exit(1);
  }

  trim_status = false;
  ta_mode     = 99;

  Constructing = true;
  typedef int (FGFDMExec::*iPMF)(void) const;
//  instance->Tie("simulation/do_trim_analysis", this, (iPMF)0, &FGFDMExec::DoTrimAnalysis);
  instance->Tie("simulation/do_simple_trim", this, (iPMF)0, &FGFDMExec::DoTrim);
  instance->Tie("simulation/terminate", (int *)&Terminate);
  Constructing = false;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGFDMExec::~FGFDMExec()
{
  try {
    checkTied( instance );
    DeAllocate();
    if (Root == 0)  delete master;
  } catch ( string msg ) {
    cout << "Caught error: " << msg << endl;
  }

  for (unsigned int i=1; i<SlaveFDMList.size(); i++) delete SlaveFDMList[i]->exec;
  SlaveFDMList.clear();

  //ToDo remove property catalog.

  Debug(1);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::Allocate(void)
{
  bool result=true;

  Atmosphere      = new FGAtmosphere(this);
  FCS             = new FGFCS(this);
  Propulsion      = new FGPropulsion(this);
  MassBalance     = new FGMassBalance(this);
  Aerodynamics    = new FGAerodynamics (this);
  Inertial        = new FGInertial(this);

  GroundCallback  = new FGGroundCallback(Inertial->GetRefRadius());

  GroundReactions = new FGGroundReactions(this);
  ExternalReactions = new FGExternalReactions(this);
  BuoyantForces   = new FGBuoyantForces(this);
  Aircraft        = new FGAircraft(this);
  Propagate       = new FGPropagate(this);
  Auxiliary       = new FGAuxiliary(this);
  Input           = new FGInput(this);

  State           = new FGState(this); // This must be done here, as the FGState
                                       // class needs valid pointers to the above
                                       // model classes

  // Initialize models so they can communicate with each other

  Atmosphere->InitModel();
  FCS->InitModel();
  Propulsion->InitModel();
  MassBalance->InitModel();
  Aerodynamics->InitModel();
  Inertial->InitModel();
  GroundReactions->InitModel();
  ExternalReactions->InitModel();
  BuoyantForces->InitModel();
  Aircraft->InitModel();
  Propagate->InitModel();
  Auxiliary->InitModel();
  Input->InitModel();

  IC = new FGInitialCondition(this);

  // Schedule a model. The second arg (the integer) is the pass number. For
  // instance, the atmosphere model could get executed every fifth pass it is called
  // by the executive. IC and Trim objects are NOT scheduled.

  Schedule(Input,           1);
  Schedule(Atmosphere,      1);
  Schedule(FCS,             1);
  Schedule(Propulsion,      1);
  Schedule(MassBalance,     1);
  Schedule(Aerodynamics,    1);
  Schedule(Inertial,        1);
  Schedule(GroundReactions, 1);
  Schedule(ExternalReactions, 1);
  Schedule(BuoyantForces,   1);
  Schedule(Aircraft,        1);
  Schedule(Propagate,       1);
  Schedule(Auxiliary,       1);

  modelLoaded = false;

  return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::DeAllocate(void)
{
  delete Input;
  delete Atmosphere;
  delete FCS;
  delete Propulsion;
  delete MassBalance;
  delete Aerodynamics;
  delete Inertial;
  delete GroundReactions;
  delete ExternalReactions;
  delete BuoyantForces;
  delete Aircraft;
  delete Propagate;
  delete Auxiliary;
  delete State;
  delete Script;

  for (unsigned i=0; i<Outputs.size(); i++) delete Outputs[i];
  Outputs.clear();

  delete IC;
  delete Trim;

  delete GroundCallback;

  FirstModel  = 0L;
  Error       = 0;

  State           = 0;
  Input           = 0;
  Atmosphere      = 0;
  FCS             = 0;
  Propulsion      = 0;
  MassBalance     = 0;
  Aerodynamics    = 0;
  Inertial        = 0;
  GroundReactions = 0;
  ExternalReactions = 0;
  BuoyantForces   = 0;
  Aircraft        = 0;
  Propagate       = 0;
  Auxiliary       = 0;
  Script          = 0;

  modelLoaded = false;
  return modelLoaded;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int FGFDMExec::Schedule(FGModel* model, int rate)
{
  FGModel* model_iterator;

  model_iterator = FirstModel;

  if (model_iterator == 0L) {                  // this is the first model

    FirstModel = model;
    FirstModel->NextModel = 0L;
    FirstModel->SetRate(rate);

  } else {                                     // subsequent model

    while (model_iterator->NextModel != 0L) {
      model_iterator = model_iterator->NextModel;
    }
    model_iterator->NextModel = model;
    model_iterator->NextModel->SetRate(rate);

  }

  return 0;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::Run(void)
{
  bool success=true;
  FGModel* model_iterator;

  model_iterator = FirstModel;
  if (model_iterator == 0L) return false;

  Debug(2);

  for (unsigned int i=1; i<SlaveFDMList.size(); i++) {
//    SlaveFDMList[i]->exec->State->Initialize(); // Transfer state to the slave FDM
//    SlaveFDMList[i]->exec->Run();
  }

  // returns true if success
  // false if complete
  if (Script != 0 && !State->IntegrationSuspended()) success = Script->RunScript();

  while (model_iterator != 0L) {
    model_iterator->Run();
    model_iterator = model_iterator->NextModel;
  }

  Frame++;
  if (!Holding()) State->IncrTime();
  if (Terminate) success = false;

  return (success);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// This call will cause the sim time to reset to 0.0

bool FGFDMExec::RunIC(void)
{
  State->SuspendIntegration();
  State->Initialize(IC);
  Run();
  State->ResumeIntegration();

  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::ResetToInitialConditions(void)
{
  FGModel* model_iterator;

  model_iterator = FirstModel;
  if (model_iterator == 0L) return;

  while (model_iterator != 0L) {
    model_iterator->InitModel();
    model_iterator = model_iterator->NextModel;
  }

  RunIC();
  if (Script) Script->ResetEvents();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::SetGroundCallback(FGGroundCallback* p)
{
  delete GroundCallback;
  GroundCallback = p;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double FGFDMExec::GetSimTime(void)
{
  return (State->Getsim_time());
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double FGFDMExec::GetDeltaT(void)
{
  return (State->Getdt());
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vector <string> FGFDMExec::EnumerateFDMs(void)
{
  vector <string> FDMList;

  FDMList.push_back(Aircraft->GetAircraftName());

  for (unsigned int i=1; i<SlaveFDMList.size(); i++) {
    FDMList.push_back(SlaveFDMList[i]->exec->GetAircraft()->GetAircraftName());
  }

  return FDMList;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::LoadScript(string script)
{
  bool result;

  Script = new FGScript(this);
  result = Script->LoadScript(script);

  return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::LoadModel(string AircraftPath, string EnginePath, string SystemsPath,
                string model, bool addModelToPath)
{
  FGFDMExec::AircraftPath = AircraftPath;
  FGFDMExec::EnginePath = EnginePath;
  FGFDMExec::SystemsPath = SystemsPath;

  return LoadModel(model, addModelToPath);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::LoadModel(string model, bool addModelToPath)
{
  string token;
  string aircraftCfgFileName;
  string separator = "/";
  Element* element = 0L;
  bool result = false; // initialize result to false, indicating input file not yet read

  modelName = model; // Set the class modelName attribute

  if( AircraftPath.empty() || EnginePath.empty() || SystemsPath.empty()) {
    cerr << "Error: attempted to load aircraft with undefined ";
    cerr << "aircraft, engine, and system paths" << endl;
    return false;
  }

  FullAircraftPath = AircraftPath;
  if (addModelToPath) FullAircraftPath += separator + model;
  aircraftCfgFileName = FullAircraftPath + separator + model + ".xml";

  if (modelLoaded) {
    DeAllocate();
    Allocate();
  }

  document = LoadXMLDocument(aircraftCfgFileName); // "document" is a class member
  if (document) {
    ReadPrologue(document);
    element = document->GetElement();

    result = true;
    while (element && result) {
      string element_name = element->GetName();
      if (element_name == "fileheader" )           result = ReadFileHeader(element);
      else if (element_name == "slave")            result = ReadSlave(element);
      else if (element_name == "metrics")          result = Aircraft->Load(element);
      else if (element_name == "mass_balance")     result = MassBalance->Load(element);
      else if (element_name == "ground_reactions") result = GroundReactions->Load(element);
      else if (element_name == "external_reactions") result = ExternalReactions->Load(element);
      else if (element_name == "buoyant_forces")   result = BuoyantForces->Load(element);
      else if (element_name == "propulsion")       result = Propulsion->Load(element);
      else if (element_name == "system")           result = FCS->Load(element,
                                                            FGFCS::stSystem);
      else if (element_name == "autopilot")        result = FCS->Load(element,
                                                            FGFCS::stAutoPilot);
      else if (element_name == "flight_control")   result = FCS->Load(element,
                                                            FGFCS::stFCS);
      else if (element_name == "aerodynamics")     result = Aerodynamics->Load(element);
      else if (element_name == "input")            result = Input->Load(element);
      else if (element_name == "output")           {
          FGOutput* Output = new FGOutput(this);
          Output->InitModel();
          Schedule(Output,       1);
          result = Output->Load(element);
          Outputs.push_back(Output);
      }
      else {
        cerr << "Found unexpected subsystem: " << element_name << ", exiting." << endl;
        result = false;
        break;
      }
      element = document->GetNextElement();
    }
  } else {
    cerr << fgred
         << "  JSBSim failed to load aircraft model."
         << fgdef << endl;
    return false;
  }

  if (result) {
    modelLoaded = true;
    Debug(3);
  } else {
    cerr << fgred
         << "  JSBSim failed to load properly."
         << fgdef << endl;
    return false;
  }

  struct PropertyCatalogStructure masterPCS;
  masterPCS.base_string = "";
  masterPCS.node = (FGPropertyManager*)Root;

  BuildPropertyCatalog(&masterPCS);

  return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::BuildPropertyCatalog(struct PropertyCatalogStructure* pcs)
{
  struct PropertyCatalogStructure* pcsNew = new struct PropertyCatalogStructure;
  int node_idx = 0;
  char int_buf[10];

  for (unsigned int i=0; i<pcs->node->nChildren(); i++) {
    pcsNew->base_string = pcs->base_string + "/" + pcs->node->getChild(i)->getName();
    node_idx = pcs->node->getChild(i)->getIndex();
    sprintf(int_buf, "[%d]", node_idx);
    if (node_idx != 0) pcsNew->base_string += string(int_buf);
    if (pcs->node->getChild(i)->nChildren() == 0) {
      if (pcsNew->base_string.substr(0,11) == string("/fdm/jsbsim")) {
        pcsNew->base_string = pcsNew->base_string.erase(0,12);
      }
      PropertyCatalog.push_back(pcsNew->base_string);
    } else {
      pcsNew->node = (FGPropertyManager*)pcs->node->getChild(i);
      BuildPropertyCatalog(pcsNew);
    }
  }
  delete pcsNew;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

string FGFDMExec::QueryPropertyCatalog(string in)
{
  string results="";
  for (unsigned i=0; i<PropertyCatalog.size(); i++) {
    if (PropertyCatalog[i].find(in) != string::npos) results += PropertyCatalog[i] + "\n";
  }
  if (results.empty()) return "No matches found\n";
  return results;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::PrintPropertyCatalog(void)
{
  cout << endl;
  cout << "  " << fgblue << highint << underon << "Property Catalog for "
       << modelName << reset << endl << endl;
  for (unsigned i=0; i<PropertyCatalog.size(); i++) {
    cout << "    " << PropertyCatalog[i] << endl;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vector <string> FGFDMExec::SPrintPropertyCatalog(void)
{
	vector <string> res;
	// char str_buf[128];
    for (unsigned i=0; i<PropertyCatalog.size(); i++) 
	{
		//sprintf(str_buf, "%s\n",PropertyCatalog[i]);
		//res += string(str_buf);
		res.push_back(PropertyCatalog[i]);
    }
	return res;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::ReadFileHeader(Element* el)
{
  bool result = true; // true for success

  if (debug_lvl & ~1) return result;

  if (el->FindElement("author"))
    cout << "  Model Author:  " << el->FindElement("author")->GetDataLine() << endl;
  if (el->FindElement("filecreationdate"))
    cout << "  Creation Date: " << el->FindElement("filecreationdate")->GetDataLine() << endl;
  if (el->FindElement("version"))
    cout << "  Version:       " << el->FindElement("version")->GetDataLine() << endl;
  if (el->FindElement("description"))
    cout << "  Description:   " << el->FindElement("description")->GetDataLine() << endl;

  return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::ReadPrologue(Element* el) // el for ReadPrologue is the document element
{
  bool result = true; // true for success

  if (!el) return false;

  string AircraftName = el->GetAttributeValue("name");
  Aircraft->SetAircraftName(AircraftName);

  if (debug_lvl & 1) cout << underon << "Reading Aircraft Configuration File"
            << underoff << ": " << highint << AircraftName << normint << endl;

  CFGVersion = el->GetAttributeValue("version");
  Release    = el->GetAttributeValue("release");

  if (debug_lvl & 1)
    cout << "                            Version: " << highint << CFGVersion
                                                    << normint << endl;
  if (CFGVersion != needed_cfg_version) {
    cerr << endl << fgred << "YOU HAVE AN INCOMPATIBLE CFG FILE FOR THIS AIRCRAFT."
            " RESULTS WILL BE UNPREDICTABLE !!" << endl;
    cerr << "Current version needed is: " << needed_cfg_version << endl;
    cerr << "         You have version: " << CFGVersion << endl << fgdef << endl;
    return false;
  }

  if (Release == "ALPHA" && (debug_lvl & 1)) {
    cout << endl << endl
         << highint << "This aircraft model is an " << fgred << Release
         << reset << highint << " release!!!" << endl << endl << reset
         << "This aircraft model may not even properly load, and probably"
         << " will not fly as expected." << endl << endl
         << fgred << highint << "Use this model for development purposes ONLY!!!"
         << normint << reset << endl << endl;
  } else if (Release == "BETA" && (debug_lvl & 1)) {
    cout << endl << endl
         << highint << "This aircraft model is a " << fgred << Release
         << reset << highint << " release!!!" << endl << endl << reset
         << "This aircraft model probably will not fly as expected." << endl << endl
         << fgblue << highint << "Use this model for development purposes ONLY!!!"
         << normint << reset << endl << endl;
  } else if (Release == "PRODUCTION" && (debug_lvl & 1)) {
    cout << endl << endl
         << highint << "This aircraft model is a " << fgblue << Release
         << reset << highint << " release." << endl << endl << reset;
  } else if (debug_lvl & 1) {
    cout << endl << endl
         << highint << "This aircraft model is an " << fgred << Release
         << reset << highint << " release!!!" << endl << endl << reset
         << "This aircraft model may not even properly load, and probably"
         << " will not fly as expected." << endl << endl
         << fgred << highint << "Use this model for development purposes ONLY!!!"
         << normint << reset << endl << endl;
  }

  return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::ReadSlave(Element* el)
{
  // Add a new slaveData object to the slave FDM list
  // Populate that slaveData element with a new FDMExec object
  // Set the IsSlave flag for that FDMExec object
  // Get the aircraft name
  // set debug level to print out no additional data for slave objects
  // Load the model given the aircraft name
  // reset debug level to prior setting

  int saved_debug_lvl = debug_lvl;
  string token;

  SlaveFDMList.push_back(new slaveData);
  SlaveFDMList.back()->exec = new FGFDMExec();
  SlaveFDMList.back()->exec->SetSlave(true);
/*
  string AircraftName = AC_cfg->GetValue("file");

  debug_lvl = 0;                 // turn off debug output for slave vehicle

  SlaveFDMList.back()->exec->SetAircraftPath( AircraftPath );
  SlaveFDMList.back()->exec->SetEnginePath( EnginePath );
  SlaveFDMList.back()->exec->SetSystemsPath( SystemsPath );
  SlaveFDMList.back()->exec->LoadModel(AircraftName);
  debug_lvl = saved_debug_lvl;   // turn debug output back on for master vehicle

  AC_cfg->GetNextConfigLine();
  while ((token = AC_cfg->GetValue()) != string("/SLAVE")) {
    *AC_cfg >> token;
    if      (token == "xloc")  { *AC_cfg >> SlaveFDMList.back()->x;    }
    else if (token == "yloc")  { *AC_cfg >> SlaveFDMList.back()->y;    }
    else if (token == "zloc")  { *AC_cfg >> SlaveFDMList.back()->z;    }
    else if (token == "pitch") { *AC_cfg >> SlaveFDMList.back()->pitch;}
    else if (token == "yaw")   { *AC_cfg >> SlaveFDMList.back()->yaw;  }
    else if (token == "roll")  { *AC_cfg >> SlaveFDMList.back()->roll;  }
    else cerr << "Unknown identifier: " << token << " in slave vehicle definition" << endl;
  }
*/
  if (debug_lvl > 0)  {
    cout << "      X = " << SlaveFDMList.back()->x << endl;
    cout << "      Y = " << SlaveFDMList.back()->y << endl;
    cout << "      Z = " << SlaveFDMList.back()->z << endl;
    cout << "      Pitch = " << SlaveFDMList.back()->pitch << endl;
    cout << "      Yaw = " << SlaveFDMList.back()->yaw << endl;
    cout << "      Roll = " << SlaveFDMList.back()->roll << endl;
  }

  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGPropertyManager* FGFDMExec::GetPropertyManager(void)
{
  return instance;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGTrim* FGFDMExec::GetTrim(void)
{
  delete Trim;
  Trim = new FGTrim(this,tNone);
  return Trim;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::DisableOutput(void)
{
  for (unsigned i=0; i<Outputs.size(); i++) {
    Outputs[i]->Disable();
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::EnableOutput(void)
{
  for (unsigned i=0; i<Outputs.size(); i++) {
    Outputs[i]->Enable();
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFDMExec::SetOutputDirectives(string fname)
{
  bool result;

  FGOutput* Output = new FGOutput(this);
  Output->SetDirectivesFile(fname);
  Output->InitModel();
  Schedule(Output,       1);
  result = Output->Load(0);
  Outputs.push_back(Output);

  return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::DoTrim(int mode)
{
  double saved_time;

  if (Constructing) return;

  if (mode < 0 || mode > JSBSim::tNone) {
    cerr << endl << "Illegal trimming mode!" << endl << endl;
    return;
  }
  saved_time = State->Getsim_time();
  FGTrim trim(this, (JSBSim::TrimMode)mode);
  if ( !trim.DoTrim() ) cerr << endl << "Trim Failed" << endl << endl;
  trim.Report();
  State->Setsim_time(saved_time);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
void FGFDMExec::DoTrimAnalysis(int mode)
{
  double saved_time;
  if (Constructing) return;

  if (mode < 0 || mode > JSBSim::taNone) {
    cerr << endl << "Illegal trimming mode!" << endl << endl;
    return;
  }
  saved_time = State->Getsim_time();

  FGTrimAnalysis trimAnalysis(this, (JSBSim::TrimAnalysisMode)mode);

  if ( !trimAnalysis.Load(IC->GetInitFile(), false) ) {
    cerr << "A problem occurred with trim configuration file " << trimAnalysis.Load(IC->GetInitFile()) << endl;
    exit(-1);
  }

  bool result = trimAnalysis.DoTrim();

  if ( !result ) cerr << endl << "Trim Failed" << endl << endl;

  trimAnalysis.Report();
  State->Setsim_time(saved_time);

  EnableOutput();
  cout << "\nOutput: " << GetOutputFileName() << endl;

}
*/
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::UseAtmosphereMSIS(void)
{
  FGAtmosphere *oldAtmosphere = Atmosphere;
  Atmosphere = new MSIS(this);
  if (!Atmosphere->InitModel()) {
    cerr << fgred << "MSIS Atmosphere model init failed" << fgdef << endl;
    Error+=1;
  }
  delete oldAtmosphere;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFDMExec::UseAtmosphereMars(void)
{
/*
  FGAtmosphere *oldAtmosphere = Atmosphere;
  Atmosphere = new FGMars(this);
  if (!Atmosphere->InitModel()) {
    cerr << fgred << "Mars Atmosphere model init failed" << fgdef << endl;
    Error+=1;
  }
  delete oldAtmosphere;
*/
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//    The bitmasked value choices are as follows:
//    unset: In this case (the default) JSBSim would only print
//       out the normally expected messages, essentially echoing
//       the config files as they are read. If the environment
//       variable is not set, debug_lvl is set to 1 internally
//    0: This requests JSBSim not to output any messages
//       whatsoever.
//    1: This value explicity requests the normal JSBSim
//       startup messages
//    2: This value asks for a message to be printed out when
//       a class is instantiated
//    4: When this value is set, a message is displayed when a
//       FGModel object executes its Run() method
//    8: When this value is set, various runtime state variables
//       are printed out periodically
//    16: When set various parameters are sanity checked and
//       a message is printed out when they go out of bounds

void FGFDMExec::Debug(int from)
{
  if (debug_lvl <= 0) return;

  if (debug_lvl & 1 && IdFDM == 0) { // Standard console startup message output
    if (from == 0) { // Constructor
      cout << "\n\n     " << highint << underon << "JSBSim Flight Dynamics Model v"
                                     << JSBSim_version << underoff << normint << endl;
      cout << halfint << "            [JSBSim-ML v" << needed_cfg_version << "]\n\n";
      cout << normint << "JSBSim startup beginning ...\n\n";
    } else if (from == 3) {
      cout << "\n\nJSBSim startup complete\n\n";
    }
  }
  if (debug_lvl & 2 ) { // Instantiation/Destruction notification
    if (from == 0) cout << "Instantiated: FGFDMExec" << endl;
    if (from == 1) cout << "Destroyed:    FGFDMExec" << endl;
  }
  if (debug_lvl & 4 ) { // Run() method entry print for FGModel-derived objects
    if (from == 2) {
      cout << "================== Frame: " << Frame << "  Time: "
           << State->Getsim_time() << " dt: " << State->Getdt() << endl;
    }
  }
  if (debug_lvl & 8 ) { // Runtime state variables
  }
  if (debug_lvl & 16) { // Sanity checking
  }
  if (debug_lvl & 64) {
    if (from == 0) { // Constructor
      cout << IdSrc << endl;
      cout << IdHdr << endl;
    }
  }
}
}


