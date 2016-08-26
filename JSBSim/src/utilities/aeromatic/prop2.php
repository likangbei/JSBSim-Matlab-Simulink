<?php

$version = 0.8;

//****************************************************
//                                                   *
// This is the CGI back-end for the Aero-Matic gen-  *
// erator. The front-end is aeromatic.html.          *
//                                                   *
// July 2003, David P. Culp, davidculp2@comcast.net  *
//                                                   * 
//****************************************************

header("Content-type: text/plain");


//***** GET DATA FROM USER ***************************

$ac_enginepower     = $_POST['ac_enginepower'];
$ac_engineunits     = $_POST['ac_engineunits'];
$ac_maxengrpm       = $_POST['ac_maxengrpm'];
$ac_pitch           = $_POST['ac_prop_pitch'];
$ac_diameter        = $_POST['ac_diameter'];
$ac_diaunits        = $_POST['ac_diaunits'];

//***** CONVERT TO ENGLISH UNITS *********************

// convert kilowatts to horsepower
if($ac_engineunits == 1) {
  $ac_enginepower *= 1.341;
  }

// convert inches to feet
if($ac_diaunits == 1) {
  $ac_diameter /= 12;
  }

// convert meters to feet
if($ac_diaunits == 2) {
  $ac_diameter *= 3.281;
  }

// find rpm which gives a tip mach of 0.88
// (static at sea level)
$max_rpm = 18763.0 / $ac_diameter;

$gearratio = $ac_maxengrpm / $max_rpm;

$max_rps = $max_rpm / 60.0;
$rps2 = $max_rps * $max_rps;
$rps3 = $rps2 * $max_rps;
$d4 = $ac_diameter * $ac_diameter * $ac_diameter * $ac_diameter;
$d5 = $d4 * $ac_diameter;
$rho = 0.002378;

// power and thrust coefficients
$cp0 = $ac_enginepower * 550.0 / $rho / $rps3 / $d5;
$ct0 = $cp0 * 0.86;

$static_thrust = $ct0 * $rho * $rps2 * $d4;

// estimate number of blades
if ($cp0 < 0.035) {
  $blades = 2;
} else if ($cp0 > 0.065) {
  $blades = 4;
} else {
  $blades = 3; 
}

// estimate moment of inertia

$L = $ac_diameter / 2;       // length each blade (feet)
$M = $L * 0.09317;       // mass each blade (slugs)
if ($L < 1) { $M = $L * 0.003; } // mass for tiny props
$ixx = $blades * (0.33333 * $M * $L * $L);

//*****************************************************
//                                                    *
//  Print XML file                                    *
//                                                    *
//*****************************************************

print("<?xml version=\"1.0\"?>\n");
print("<!-- Generated by Aero-Matic v $version\n\n");
print("     Inputs:\n"); 
print("                horsepower: $ac_enginepower\n");
if($ac_pitch == 0)
  print("                     pitch: fixed\n");
else
  print("                     pitch: variable\n");
print("            max engine rpm: $ac_maxengrpm\n");
print("        prop diameter (ft): $ac_diameter\n");
print("\n     Outputs:\n");
printf("              max prop rpm:%7.2f\n", $max_rpm);
printf("                gear ratio:%7.2f\n", $gearratio);
printf("                       Cp0:%7.6f\n", $cp0);
printf("                       Ct0:%7.6f\n", $ct0);
printf("       static thrust (lbs):%7.2f\n", $static_thrust);
print("-->\n\n");

print("<propeller name=\"prop\">\n");
printf("  <ixx>%7.4f </ixx>\n", $ixx);
printf("  <diameter unit=\"IN\">%5.1f </diameter>\n", $ac_diameter * 12);
print("  <numblades> $blades </numblades>\n");
printf("  <gearratio>%4.2f </gearratio>\n", $gearratio);

if($ac_pitch == 0) {
  //print("  <minpitch> 22 </minpitch>\n");
  //print("  <maxpitch> 22 </maxpitch>\n");
 } else {
  print("  <minpitch> 12 </minpitch>\n");
  print("  <maxpitch> 30 </maxpitch>\n");
 }

if($ac_pitch == 1) {
  printf("  <minrpm>%7.0f </minrpm>\n", $max_rpm * 0.85);
  printf("  <maxrpm>%7.0f </maxrpm>\n", $max_rpm);
}

print("\n");

if($ac_pitch == 0) {          // fixed pitch
 print("  <table name=\"C_THRUST\" type=\"internal\">\n");
 print("     <tableData>\n");
 printf("       0.0  %5.4f\n", $ct0 * 1.0);
 printf("       0.1  %5.4f\n", $ct0 * 0.959);
 printf("       0.2  %5.4f\n", $ct0 * 0.917);
 printf("       0.3  %5.4f\n", $ct0 * 0.844);
 printf("       0.4  %5.4f\n", $ct0 * 0.758);
 printf("       0.5  %5.4f\n", $ct0 * 0.668);
 printf("       0.6  %5.4f\n", $ct0 * 0.540);
 printf("       0.7  %5.4f\n", $ct0 * 0.410);
 printf("       0.8  %5.4f\n", $ct0 * 0.222);
 printf("       1.0  %5.4f\n", $ct0 * -0.075);
 printf("       1.2  %5.4f\n", $ct0 * -0.394);
 printf("       1.4  %5.4f\n", $ct0 * -0.708);
 print("     </tableData>\n");
 print("  </table>\n\n");
} else {                      // variable pitch
 print("  <table name=\"C_THRUST\" type=\"internal\">\n");
 print("      <tableData>\n");
 print("                12         30\n");
 $ct30 = $ct0 + 0.04;
 printf("       0.0      %5.4f   %5.4f\n", $ct0 * 1.0, $ct30 * 1.0);
 printf("       0.1      %5.4f   %5.4f\n", $ct0 * 0.917, $ct30 * 1.0);
 printf("       0.2      %5.4f   %5.4f\n", $ct0 * 0.833, $ct30 * 1.0);
 printf("       0.3      %5.4f   %5.4f\n", $ct0 * 0.692, $ct30 * 0.995);
 printf("       0.4      %5.4f   %5.4f\n", $ct0 * 0.526, $ct30 * 0.990);
 printf("       0.5      %5.4f   %5.4f\n", $ct0 * 0.359, $ct30 * 0.977);
 printf("       0.6      %5.4f   %5.4f\n", $ct0 * 0.154, $ct30 * 0.925);
 printf("       0.7      %5.4f   %5.4f\n", $ct0 * -0.013, $ct30 * 0.832);
 printf("       0.8      %5.4f   %5.4f\n", $ct0 * -0.295, $ct30 * 0.738);
 printf("       1.0      %5.4f   %5.4f\n", $ct0 * -0.641, $ct30 * 0.491);
 printf("       1.2      %5.4f   %5.4f\n", $ct0 * -1.050, $ct30 * 0.262);
 printf("       1.4      %5.4f   %5.4f\n", $ct0 * -1.436, $ct30 * 0.019);
 printf("       1.6      %5.4f   %5.4f\n", $ct0 * -1.880, $ct30 * -0.215);
 printf("       1.8      %5.4f   %5.4f\n", $ct0 * -2.300, $ct30 * -0.448);
 printf("       2.0      %5.4f   %5.4f\n", $ct0 * -2.700, $ct30 * -0.692);
 printf("       2.2      %5.4f   %5.4f\n", $ct0 * -3.100, $ct30 * -0.940);
 printf("       2.4      %5.4f   %5.4f\n", $ct0 * -3.500, $ct30 * -1.190);
 print("      </tableData>\n");
 print("  </table>\n");
}

print("\n");
if($ac_pitch == 0) {          // fixed pitch
 print("  <table name=\"C_POWER\" type=\"internal\">\n");
 print("     <tableData>\n");
 printf("       0.0  %5.4f\n", $cp0 * 1.0);
 printf("       0.1  %5.4f\n", $cp0 * 1.0);
 printf("       0.2  %5.4f\n", $cp0 * 0.976);
 printf("       0.3  %5.4f\n", $cp0 * 0.953);
 printf("       0.4  %5.4f\n", $cp0 * 0.898);
 printf("       0.5  %5.4f\n", $cp0 * 0.823);
 printf("       0.6  %5.4f\n", $cp0 * 0.755);
 printf("       0.7  %5.4f\n", $cp0 * 0.634);
 printf("       0.8  %5.4f\n", $cp0 * 0.518);
 printf("       1.0  %5.4f\n", $cp0 * 0.185);
 printf("       1.2  %5.4f\n", $cp0 * -0.296);
 printf("       1.4  %5.4f\n", $cp0 * -0.890);
 printf("       1.6  %5.4f\n", $cp0 * -1.511);
 print("     </tableData>\n");
 print("  </table>\n");
} else {                      // variable pitch
 print("  <table name=\"C_POWER\" type=\"internal\">\n");
 print("     <tableData>\n");
 print("                12         30\n");
 $cp30 = $cp0 + 0.06;
 printf("       0.0      %5.4f   %5.4f\n", $cp0 * 1.0, $cp30 * 1.0);
 printf("       0.1      %5.4f   %5.4f\n", $cp0 * 1.0, $cp30 * 1.0);
 printf("       0.2      %5.4f   %5.4f\n", $cp0 * 0.953, $cp30 * 1.0);
 printf("       0.3      %5.4f   %5.4f\n", $cp0 * 0.906, $cp30 * 1.0);
 printf("       0.4      %5.4f   %5.4f\n", $cp0 * 0.797, $cp30 * 1.0);
 printf("       0.5      %5.4f   %5.4f\n", $cp0 * 0.656, $cp30 * 0.989);
 printf("       0.6      %5.4f   %5.4f\n", $cp0 * 0.531, $cp30 * 0.978);
 printf("       0.7      %5.4f   %5.4f\n", $cp0 * 0.313, $cp30 * 0.956);
 printf("       0.8      %5.4f   %5.4f\n", $cp0 * 0.125, $cp30 * 0.911);
 printf("       1.0      %5.4f   %5.4f\n", $cp0 * -0.375, $cp30 * 0.744);
 printf("       1.2      %5.4f   %5.4f\n", $cp0 * -1.093, $cp30 * 0.500);
 printf("       1.4      %5.4f   %5.4f\n", $cp0 * -2.030, $cp30 * 0.250);
 printf("       1.6      %5.4f   %5.4f\n", $cp0 * -3.0, $cp30 * -0.022);
 printf("       1.8      %5.4f   %5.4f\n", $cp0 * -4.0, $cp30 * -0.610);
 printf("       2.0      %5.4f   %5.4f\n", $cp0 * -5.0, $cp30 * -1.220);
 printf("       2.2      %5.4f   %5.4f\n", $cp0 * -6.0, $cp30 * -1.830);
 printf("       2.4      %5.4f   %5.4f\n", $cp0 * -7.0, $cp30 * -2.440);
 print("     </tableData>\n");
 print("  </table>\n");
}

print("\n</propeller>\n");

?>
