/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGExternalReactions.cpp
 Author:       David P. Culp
 Date started: 17/11/06
 Purpose:      Manages the External Forces
 Called by:    FGAircraft

 ------------- Copyright (C) 2006  David P. Culp (davidculp2@comcast.net) -------------

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

HISTORY
--------------------------------------------------------------------------------
17/11/06   DC   Created

/%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "FGExternalReactions.h"
#include <string>

namespace JSBSim {

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
DEFINITIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
GLOBAL DATA
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

static const char *IdSrc = "$Id: FGExternalReactions.cpp,v 1.5 2008/05/31 23:13:29 jberndt Exp $";
static const char *IdHdr = ID_EXTERNALREACTIONS;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

FGExternalReactions::FGExternalReactions(FGFDMExec* fdmex) : FGModel(fdmex)
{
  NoneDefined = true;
  Debug(0);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGExternalReactions::Load(Element* el)
{
  Debug(2);

  // Interface properties are all stored in the interface properties array.
  // ToDo: Interface properties should not be created if they already exist.
  // A check should be done prior to creation. This ought to make it easier 
  // to work with FlightGear, where some properties used in definitions may 
  // already have been created, but would not be seen when JSBSim is run
  // in standalone mode.

  Element* property_element;
  property_element = el->FindElement("property");
  while (property_element) {
    double value=0.0;
    if ( ! property_element->GetAttributeValue("value").empty())
      value = property_element->GetAttributeValueAsNumber("value");
    interface_properties.push_back(new double(value));
    string interface_property_string = property_element->GetDataLine();
    PropertyManager->Tie(interface_property_string, interface_properties.back());
    property_element = el->FindNextElement("property");
  }

  // Parse force elements

  int index=0;
  Element* force_element = el->FindElement("force");
  while (force_element) {
    Forces.push_back( new FGExternalForce(FDMExec, force_element, index) );
    NoneDefined = false;
    index++; 
    force_element = el->FindNextElement("force");
  }

  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGExternalReactions::~FGExternalReactions()
{
  for (unsigned int i=0; i<Forces.size(); i++) delete Forces[i];
  Forces.clear();
  for (unsigned int i=0; i<interface_properties.size(); i++) delete interface_properties[i];
  interface_properties.clear();
  Debug(1);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGExternalReactions::InitModel(void)
{
  if (!FGModel::InitModel()) return false;

  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGExternalReactions::Run()
{
  if (FGModel::Run()) return true;
  if (FDMExec->Holding()) return false; // if paused don't execute
  if (NoneDefined) return true;

  vTotalForces.InitMatrix();
  vTotalMoments.InitMatrix();

  for (unsigned int i=0; i<Forces.size(); i++) {
    vTotalForces  += Forces[i]->GetBodyForces();
    vTotalMoments += Forces[i]->GetMoments();
  }

  return false;
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

void FGExternalReactions::Debug(int from)
{
  if (debug_lvl <= 0) return;

  if (debug_lvl & 1) { // Standard console startup message output
    if (from == 0) { // Constructor - loading and initialization
    }
    if (from == 2) { // Loading
      cout << endl << "  External Reactions: " << endl;
    }
  }
  if (debug_lvl & 2 ) { // Instantiation/Destruction notification
    if (from == 0) cout << "Instantiated: FGExternalReactions" << endl;
    if (from == 1) cout << "Destroyed:    FGExternalReactions" << endl;
  }
  if (debug_lvl & 4 ) { // Run() method entry print for FGModel-derived objects
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

} // namespace JSBSim

