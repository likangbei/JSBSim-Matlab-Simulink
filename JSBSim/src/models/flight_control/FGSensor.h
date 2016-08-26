/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Header:       FGSensor.h
 Author:       Jon Berndt
 Date started: 9 July 2005

 ------------- Copyright (C) 2005 -------------

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

HISTORY
--------------------------------------------------------------------------------

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
SENTRY
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef FGSENSOR_H
#define FGSENSOR_H

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "FGFCSComponent.h"
#include <input_output/FGXMLElement.h>

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
DEFINITIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#define ID_SENSOR "$Id: FGSensor.h,v 1.10 2008/05/01 01:03:14 dpculp Exp $"

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
FORWARD DECLARATIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

namespace JSBSim {

class FGFCS;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS DOCUMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/** Encapsulates a Sensor component for the flight control system.

Syntax:

@code
<sensor name="name">
  <input> property </input>
  <lag> number </lag>
  <noise variation="PERCENT|ABSOLUTE"> number </noise>
  <quantization name="name">
    <bits> number </bits>
    <min> number </min>
    <max> number </max>
  </quantization>
  <drift_rate> number </drift_rate>
  <bias> number </bias>
</sensor>
@endcode

Example:

@code
<sensor name="aero/sensor/qbar">
  <input> aero/qbar </input>
  <lag> 0.5 </lag>
  <noise variation="PERCENT"> 2 </noise>
  <quantization name="aero/sensor/quantized/qbar">
    <bits> 12 </bits>
    <min> 0 </min>
    <max> 400 </max>
  </quantization>
  <bias> 0.5 </bias>
</sensor>
@endcode

The only required element in the sensor definition is the input element. In that
case, no degradation would be modeled, and the output would simply be the input.

For noise, if the type is PERCENT, then the value supplied is understood to be a
percentage variance. That is, if the number given is 0.05, the the variance is
understood to be +/-0.05 percent maximum variance. So, the actual value for the sensor
will be *anywhere* from 0.95 to 1.05 of the actual "perfect" value at any time -
even varying all the way from 0.95 to 1.05 in adjacent frames - whatever the delta
time.

@author Jon S. Berndt
@version $Revision: 1.10 $
*/

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS DECLARATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

class FGSensor  : public FGFCSComponent
{
public:
  FGSensor(FGFCS* fcs, Element* element);
  ~FGSensor();

  inline void SetFailLow(double val) {if (val > 0.0) fail_low = true; else fail_low = false;}
  inline void SetFailHigh(double val) {if (val > 0.0) fail_high = true; else fail_high = false;}
  inline void SetFailStuck(double val) {if (val > 0.0) fail_stuck = true; else fail_stuck = false;}

  inline double GetFailLow(void) const {if (fail_low) return 1.0; else return 0.0;}
  inline double GetFailHigh(void) const {if (fail_high) return 1.0; else return 0.0;}
  inline double GetFailStuck(void) const {if (fail_stuck) return 1.0; else return 0.0;}
  inline int    GetQuantized(void) const {return quantized;}

  bool Run (void);

private:
  enum eNoiseType {ePercent=0, eAbsolute} NoiseType;
  double dt;
  double min, max;
  double span;
  double bias;
  double drift_rate;
  double drift;
  double noise_variance;
  double lag;
  double granularity;
  double ca; /// lag filter coefficient "a"
  double cb; /// lag filter coefficient "b"
  double PreviousOutput;
  double PreviousInput;
  int noise_type;
  int bits;
  int quantized;
  int divisions;
  bool fail_low;
  bool fail_high;
  bool fail_stuck;
  string quant_property;

  void Noise(void);
  void Bias(void);
  void Drift(void);
  void Quantize(void);
  void Lag(void);

  void bind(void);

  void Debug(int from);
};
}
#endif
