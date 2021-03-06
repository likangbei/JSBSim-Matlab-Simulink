% /* JSBSim SFunction GUI version 0.3
%  * The state vector has been simplified to 12 states instead of 19.  This has been done as a result
%  * of needing to simplify the state vector for the upcoming trim and linearization functions
%  * The calculated outputs vector has now been expanded to show some of the data missing from the state
%  * vector, with the exception of the quaternoins, which may be added later.
%  * The new GUI allows convenient control of all
%  * initialization parameters using both sliders and text boxes.  The
%  * simulink model, aircraft model, verbosity, JSBSim delta time, initial controls,
%  * and initial states can all be controlled through the GUI.  
%  * A JSBSim Multiplier function has been added to increase the execution rate of the simulation.
%  * This allows JSBSim to complete from 1 to 100 simulation cycles for every Simulink cycle.
%  * **************************************************************************************************
%  * Bug fixes
%  * %%% Fixed issues with Debug Verbosity settings
%  * %%% Fixed problem with "verbose" Verbosity setting that did not allow simulation to run properly
%  * %%% Fixed issue with throttles not being initialized properly and angines not being properly spooled up to the 
%  * intended power setting
%  *
%  * 01/22/10 Brian Mills
%  * 
%  * *********Discrete States Version******************************************************************
%  * JSBSim calculates states.  NO integration performed by Simulink.
%  * Use fixed step discrete state solver
%  * Basic implementation of a JSBSim S-Function that takes 5 input parameters
%  * at the S-Function's block parameters dialog box:
%  * 'ac_name_string', 
%  * [u-fps v-fps w-fps p-radsec q-radsec r-radsec h-sl-ft long-gc-deg lat-gc-deg 
%  *   phi-rad theta-rad psi-rad],
%  * [throttle-cmd-norm aileron-cmd-norm elevator-cmd-norm rudder-cmd-norm mixture-cmd-norm set-running flaps-cmd-norm gear-cmd-norm],
%  * [delta_T], 'verbosity'
%  * Verbosity can either be set to 'Silent', 'Verbose', 'VeryVerbose' or 'Debug'
%  * The model currently takes 8 control inputs:throttle, aileron, elevator, rudder, mixture, set-running, flaps and gear.
%  * The model has 12 states:[u-fps v-fps w-fps p-rad-sec q-rad-sec r-rad-sec h-sl-ft long-deg lat-deg phi-rad theta-rad psi-rad] 
%  * Model has 4 output ports: state vector, control output vector, propulsion output vector and calculated output vector.
%  * States output [u-fps v-fps w-fps p-rad-sec q-rad-sec r-rad-sec h-sl-ft long-deg lat-deg phi-rad theta-rad psi-rad] 
%  * Flight Controls output [thr-pos-norm left-ail-pos-rad el-pos-rad tvc-pos-rad rud-pos-rad flap-pos-norm right-ail-pos-rad 
%  * speedbrake-pos-rad spoiler-pos-rad lef-pos-rad gear-pos-norm Nose-gear-steering-pos-deg gear-unit-WOW]
%  * Propulsion output piston (per engine) [prop-rpm prop-thrust-lbs mixture fuel-flow-gph advance-ratio power-hp pt-lbs_sqft 
%  * volumetric-efficiency bsfc-lbs_hphr prop-torque blade-angle prop-pitch]
%  * Propulsion output turbine (per engine) [thrust-lbs n1 n2 fuel-flow-pph fuel-flow-pps pt-lbs_sqft pitch-rad reverser-rad yaw-rad inject-cmd 
%  * set-running fuel-dump]
%  * Calculated outputs [pilot-Nz alpha-rad alpha-dot-rad-sec beta-rad beta-dot-rad-sec vc-fps vc-kts 
%  *						Vt-fps vg-fps mach climb-rate-fps]
%  *
%  * The UpdateStates method added to JSBSimInterface is called for every s-function simulation time step.
%  * Currently it is advised that if the AC model FCS has integrators, then after each simulation run "clearSF" 
%  * should be entered at the Matlab command line to reset the simulation.
%  * This will ensure that every consecutive simulation run starts from the same initial states.  
%  * It is planned to fix this in the near future.
%  * Please look in the mdlInitializeSizes method for more detailed input port and output port details.
%  * *************************************************************************************************************************
%  * *************************************************************************************************************************
%  * 08/08/09 JSBSimSFunction revision 1.0 for campatibility with JSBSim 1.0
%  * Brian Mills
%  * *************************************************************************************************************************
%  * JSBSimInterface written by Agostino De Marco for use in the JSBSimMexFunction project. Additional functions have been added and changes 
%  * made to work with SFunction API. Thanks to Agostino for providing the basis for this project.
%  * *************************************************************************************************************************