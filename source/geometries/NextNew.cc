// ----------------------------------------------------------------------------
//  $Id: NextNew.cc  $
//
//  Author:  Miquel Nebot Guinot <miquel.nebot@ific.uv.es>
//           <jmunoz@ific.uv.es>, <justo.martin-albo@ific.uv.es>   
//  Created: Sept 2013
//  
//  Copyright (c) 2013 NEXT Collaboration
// ---------------------------------------------------------------------------- 

#include "NextNew.h"

#include "Next100Shielding.h"
#include "NextNewPedestal.h"
//#include "NextNewCuCastle.h"
#include "NextNewVessel.h"
#include "NextNewIcs.h"
#include "NextNewInnerElements.h"
//#include "Na22Source.h"
#include "BoxPointSampler.h"
#include "MuonsPointSampler.h"
#include "OpticalMaterialProperties.h"
#include "Visibilities.h"
#include "IonizationSD.h"
#include "CalibrationSource.h"

#include <G4GenericMessenger.hh>
#include <G4Box.hh>
#include <G4Tubs.hh>
#include <G4Material.hh>
#include <G4LogicalVolume.hh>
#include <G4PVPlacement.hh>
#include <G4VisAttributes.hh>
#include <G4UserLimits.hh>
#include <G4NistManager.hh>
#include <G4RotationMatrix.hh>
#include <G4Transform3D.hh>
#include <G4SDManager.hh>
#include <G4SubtractionSolid.hh>
#include <G4UnionSolid.hh>

#include <CLHEP/Units/SystemOfUnits.h>
#include <CLHEP/Units/PhysicalConstants.h>
#include <stdexcept>

namespace nexus {

  using namespace CLHEP;

  NextNew::NextNew():
    BaseGeometry(),
    // Lab dimensions
    _lab_size (5. * m),
    _rot_angle(pi),
    _lead_block(false),
    _lead_dist(0.*mm),
    _dist_scint(20.*cm)
    //   _ext_source_distance(0.*mm)
    // Buffer gas dimensions
  {
    //Shielding
    _shielding = new Next100Shielding();
    //Pedestal
    _pedestal = new NextNewPedestal();
    //Copper Castle
    // _cu_castle = new NextNewCuCastle();
    
    //Vessel
    _vessel = new NextNewVessel();
    //ICS
    _ics = new NextNewIcs();   
    //Inner elements
    _inner_elements = new NextNewInnerElements();

    _msg = new G4GenericMessenger(this, "/Geometry/NextNew/", "Control commands of geometry NextNew.");
    _msg->DeclareProperty("lead_block", _lead_block, "Block of lead on the lateral port");
    _msg->DeclareProperty("lead_distance", _lead_dist, "Distance between the two blocks of lead");
    _msg->DeclareProperty("scint_distance", _dist_scint, "Distance between the end of the lateral port tube and the external scintillator");
    
    _cal = new CalibrationSource();
    _cal->Construct();
  }

  NextNew::~NextNew()
  {
    //deletes
    delete _shielding;
    delete _pedestal;
    // delete _cu_castle;
   
    delete _vessel;
    delete _ics;
    delete _inner_elements;
    
    delete _lab_gen;

    delete _muon_gen;
  }

  void NextNew::BuildExtScintillator(G4ThreeVector pos)
{ 
  //G4double dist_sc = 5.*cm;
  G4double radius = 44.5/2.*mm;
  G4double length = 38.7*mm;
  
  G4Tubs* sc_solid = new G4Tubs("NaI", 0., radius, length/2., 0., twopi);
  G4Material* mat = 
    G4NistManager::Instance()->FindOrBuildMaterial("G4_SODIUM_IODIDE");
  G4LogicalVolume* sc_logic = new G4LogicalVolume(sc_solid, mat, "NaI");
  sc_logic->SetUserLimits(new G4UserLimits(1.*mm));

  G4RotationMatrix rot;
  rot.rotateY(-pi/2.);

  G4ThreeVector pos_scint = 
    G4ThreeVector(pos.getX() + length/2., pos.getY(), pos.getZ()); 
  new G4PVPlacement(G4Transform3D(rot, pos_scint), sc_logic, "NaI",
		    _shielding_air_logic, false, 0, false);
  G4VisAttributes yellow_col = nexus::Yellow();
  yellow_col.SetForceSolid(true);
  sc_logic->SetVisAttributes(yellow_col);
  

  // NaI is defined as an ionization sensitive volume.
  G4SDManager* sdmgr = G4SDManager::GetSDMpointer();
  G4String detname = "/NEXTNEW/NAI";
  IonizationSD* ionisd = new IonizationSD(detname);
  ionisd->IncludeInTotalEnergyDeposit(false);
  sdmgr->AddNewDetector(ionisd);
  sc_logic->SetSensitiveDetector(ionisd);
  

}

 void NextNew::Construct()
  {
    // LAB /////////////////////////////////////////////////////////////
    // This is just a volume of air surrounding the detector so that events 
    //(from calibration sources or cosmic rays) can be generated on the outside.
    
    G4Box* lab_solid = 
      new G4Box("LAB", _lab_size/2., _lab_size/2., _lab_size/2.);
    
    _lab_logic = new G4LogicalVolume(lab_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_AIR"), "LAB");
    
    _lab_logic->SetVisAttributes(G4VisAttributes::Invisible);

    // Set this volume as the wrapper for the whole geometry 
    // (i.e., this is the volume that will be placed in the world)
    this->SetLogicalVolume(_lab_logic);
    
    //SHIELDING
    _shielding->Construct();
    G4LogicalVolume* shielding_logic = _shielding->GetLogicalVolume();

    _shielding_air_logic = _shielding->GetAirLogicalVolume();
  
    //PEDESTAL
    _pedestal->SetLogicalVolume(_shielding_air_logic);
    _pedestal->Construct();
    
    //VESSEL
    _vessel->Construct();
    G4LogicalVolume* vessel_logic = _vessel->GetLogicalVolume();
    G4ThreeVector position(0.,0.,0.); 
    new G4PVPlacement(0, position, vessel_logic, 
		      "VESSEL", _shielding_air_logic, false, 0, false);
    G4LogicalVolume* vessel_gas_logic = _vessel->GetInternalLogicalVolume();

     //INNER ELEMENTS
    _inner_elements->SetLogicalVolume(vessel_gas_logic);
    _inner_elements->Construct();

    _shielding->SetELzCoord(_inner_elements->GetELzCoord());
    _vessel->SetELzCoord(_inner_elements->GetELzCoord());

     //ICS
    _ics->SetLogicalVolume(vessel_gas_logic);
    _ics->SetNozzlesZPosition( _vessel->GetLATNozzleZPosition(),_vessel->GetUPNozzleZPosition());
    _ics->SetELzCoord(_inner_elements->GetELzCoord());
    _ics->Construct();

    //COPPER CASTLE 
    // _cu_castle->SetLogicalVolume(_buffer_gas_logic);
    // _cu_castle->SetELzCoord(_inner_elements->GetELzCoord());
    // _cu_castle->Construct();

    G4ThreeVector lat_pos = _vessel->GetLatExtSourcePosition(); // this is the position of the end of the port tube
    G4RotationMatrix* lat_rot = new G4RotationMatrix();
    lat_rot->rotateY(-pi/2);

    /*
   /// In principle we shouldn't use this source anymore, but never say never...
    Na22Source* na22 = new Na22Source();
    na22->Construct();
    G4LogicalVolume* na22_logic = na22->GetLogicalVolume();

    // This is the position of the whole Na22 source + plastic support.
    G4ThreeVector lat_pos_source = G4ThreeVector(lat_pos.getX() + na22->GetSupportThickness()/2., lat_pos.getY(), lat_pos.getZ());

    new G4PVPlacement(G4Transform3D(*lat_rot, lat_pos_source), na22_logic, "NA22_SOURCE",
    		      _shielding_air_logic, false, 0, false);

    G4ThreeVector up_pos = _vessel->GetUpExtSourcePosition(); // this is the position of the end of the port tube
    G4RotationMatrix* up_rot = new G4RotationMatrix();
    up_rot->rotateX(pi/2.);

    G4ThreeVector up_pos_source = G4ThreeVector(up_pos.getX() , up_pos.getY() + na22->GetSupportThickness()/2., up_pos.getZ());

    new G4PVPlacement(G4Transform3D(*up_rot, up_pos_source), na22_logic, "NA22_SOURCE",
		      _shielding_air_logic, false, 1, false);

    G4VisAttributes light_brown_col = nexus::CopperBrown();
    na22_logic->SetVisAttributes(light_brown_col);
    */
    
    // Build NaI external scintillator
    BuildExtScintillator(G4ThreeVector(lat_pos.getX() + _dist_scint, lat_pos.getY(), lat_pos.getZ()));

    G4ThreeVector source_pos;

    if (_lead_block) {

      G4double offset_sub = 1.*mm;

       // Freddy lead block around nozzle

      G4double lead_size_horizontal = 103.*mm;
      G4double lead_size_vertical = 100.*mm;

      G4double vessel_out_diam = 664*mm;
      G4double lateral_nozzle_flange_diam = 75.*mm;
      G4double lateral_port_tube_diam = 10.*mm;
      G4double lateral_port_tube_thick = 1.*mm;

      G4double middle_lead_thick = 50.4*mm;
      G4double outer_and_middle_lead_thick = 75.3*mm;
      
      G4double inner_lead_thick = 16.*mm;
      G4double screw_lead_thick = 5.*mm;
      G4double nozzle_lead_thick = inner_lead_thick - 5.*mm;
      
      G4double lateral_nozzle_diam = 30.*mm + 2.*3.*mm;
      // G4double offset_sub = 1.*mm;

      // Piece of lead around nozzle
      G4Box* lead_nozzle_solid = new G4Box("LEAD_BLOCK_NOZZLE_FULL", lead_size_horizontal/2., lead_size_vertical/2.,  nozzle_lead_thick/2.);
      G4Tubs* lateral_nozzle_solid = 
       	new G4Tubs("NOZZLE_INNER", 0., lateral_nozzle_diam/2., (nozzle_lead_thick + offset_sub)/2., 0., twopi);    
      G4SubtractionSolid* lead_nozzle_freddy_solid =
	new G4SubtractionSolid("LEAD_BLOCK_NOZZLE", lead_nozzle_solid, lateral_nozzle_solid, 0 , G4ThreeVector(0.,0.,0.));
      G4LogicalVolume* lead_nozzle_freddy_logic =
	new G4LogicalVolume(lead_nozzle_freddy_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb"), "LEAD_BLOCK_NOZZLE");
      G4ThreeVector block_nozzle_pos =
       	G4ThreeVector(vessel_out_diam/2. + nozzle_lead_thick/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, block_nozzle_pos), lead_nozzle_freddy_logic, "LEAD_BLOCK_NOZZLE",
			_shielding_air_logic, false, 0, false);

      // Piece of lead around screws
      G4Box* lead_screw_flange_solid = new G4Box("LEAD_BLOCK_SCREW_FLANGE_FULL", lead_size_horizontal/2., lead_size_vertical/2.,  screw_lead_thick/2.);
      G4Tubs* screw_flange_solid = 
      	new G4Tubs("SCREW_FLANGE_INNER", 0., lateral_nozzle_flange_diam/2., (screw_lead_thick + offset_sub)/2., 0., twopi);      
      G4SubtractionSolid* lead_screw_freddy_solid =
       	new G4SubtractionSolid("LEAD_BLOCK_SCREW_FLANGE", lead_screw_flange_solid, screw_flange_solid, 0 , G4ThreeVector(0., 0., 0.));
      G4LogicalVolume* lead_screw_freddy_logic =
	new G4LogicalVolume(lead_screw_freddy_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb"), "LEAD_BLOCK_SCREW_FLANGE");
      G4ThreeVector block_screw_flange_pos =
	G4ThreeVector(vessel_out_diam/2. + nozzle_lead_thick + screw_lead_thick/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, block_screw_flange_pos),lead_screw_freddy_logic,"LEAD_BLOCK_SCREW_FLANGE",
			_shielding_air_logic, false, 0, false);

       // Middle piece of lead
      G4Box* lead_middle_solid = new G4Box("LEAD_BLOCK_MIDDLE_FULL", lead_size_horizontal/2., lead_size_vertical/2.,  middle_lead_thick/2.);
      G4Tubs* lateral_port_tube_middle_solid = 
	new G4Tubs("LAT_PORT_TUBE_MIDDLE", 0., lateral_nozzle_flange_diam/2., (middle_lead_thick + offset_sub)/2., 0., twopi);
      G4SubtractionSolid* lead_middle_freddy_solid =
	new G4SubtractionSolid("LEAD_BLOCK_MIDDLE", lead_middle_solid, lateral_port_tube_middle_solid, 0 , G4ThreeVector(0.,0.,0.));
      G4LogicalVolume* lead_middle_freddy_logic =
	new G4LogicalVolume(lead_middle_freddy_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb"), "LEAD_BLOCK");
      G4ThreeVector block_middle_pos =
	G4ThreeVector(vessel_out_diam/2. + nozzle_lead_thick + screw_lead_thick + middle_lead_thick/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, block_middle_pos), lead_middle_freddy_logic, "LEAD_BLOCK_MIDDLE",
			_shielding_air_logic, false, 0, false);

 
      // Outer piece of lead
      G4double outer_lead_thick = outer_and_middle_lead_thick- middle_lead_thick;
      G4Box* lead_out_solid =
	new G4Box("LEAD_BLOCK_OUT_FULL", lead_size_horizontal/2., lead_size_vertical/2., outer_lead_thick/2.);
      G4Tubs* lateral_port_tube_out_solid = 
      	new G4Tubs("LAT_PORT_TUBE_OUT", 0., (lateral_port_tube_diam + 2.*lateral_port_tube_thick)/2., (outer_lead_thick + offset_sub)/2., 0., twopi);
      G4SubtractionSolid* lead_out_freddy_solid =
	new G4SubtractionSolid("LEAD_BLOCK_OUT", lead_out_solid, lateral_port_tube_out_solid, 0 , G4ThreeVector(0.,0.,0.));
      G4LogicalVolume* lead_out_freddy_logic =
	new G4LogicalVolume(lead_out_freddy_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb"), "LEAD_BLOCK");
      G4ThreeVector block_out_pos =
	G4ThreeVector(vessel_out_diam/2. + nozzle_lead_thick + screw_lead_thick + middle_lead_thick + outer_lead_thick/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, block_out_pos), lead_out_freddy_logic, "LEAD_BLOCK_OUT",
      			_shielding_air_logic, false, 0, false);
      
      // Jordi lead collimator block     
      G4double diam = 5.*mm;     
      G4double coll_thick = 50.*mm;
      G4Box* lead_coll_full_solid = 
	new G4Box("LEAD_COLL", 100.*mm/2., 100.*mm/2.,  coll_thick/2.);
      G4Tubs* cylinder_coll_1_solid = 
       	new G4Tubs("CYLINDER_COLL1", 0., diam/2., (coll_thick + offset_sub)/2., 0., twopi); 
      G4SubtractionSolid* lead_coll_solid =
	new G4SubtractionSolid("LEAD_COLL",
                               lead_coll_full_solid, cylinder_coll_1_solid, 
			       0 , G4ThreeVector(0.,0.,0.));
      // G4Tubs* cylinder_coll_2_solid = 
      //  	new G4Tubs("CYLINDER_COLL2", 0., _cal->GetCapsuleDiameter()/2., 
      //   	   (_cal->GetCapsuleThickness() + offset_sub)/2., 0., twopi); 
      // G4SubtractionSolid* lead_coll_solid =
      //   new G4SubtractionSolid("LEAD_COLL", lead_coll_1_solid, cylinder_coll_2_solid, 
      //   		       0 , G4ThreeVector(0.,0.,-_lead_thick/2. + (_cal->GetCapsuleThickness() - offset_sub)/2.));
      G4LogicalVolume* lead_coll_logic =
	new G4LogicalVolume(lead_coll_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb"), "LEAD_COLL");
      G4ThreeVector lead_coll_pos =
	G4ThreeVector(block_out_pos.getX() + outer_lead_thick/2. + _lead_dist + coll_thick/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, lead_coll_pos), lead_coll_logic, 
			"LEAD_COLL", _shielding_air_logic, false, 0, false);


      // Jordi lead source block     
      diam = 10.*mm;     
      G4double source_thick = 48.*mm;
      G4Box* lead_source_full_solid = 
	new G4Box("LEAD_SOURCE", 100.*mm/2., 100.*mm/2.,  source_thick/2.);
      G4Tubs* cylinder_coll_2_solid = 
       	new G4Tubs("CYLINDER_COLL2", 0., diam/2., (source_thick + offset_sub)/2., 0., twopi); 
      G4SubtractionSolid* lead_source_solid =
	new G4SubtractionSolid("LEAD_SOURCE",
                               lead_source_full_solid, cylinder_coll_2_solid, 
			       0 , G4ThreeVector(0.,0.,0.));
   
      G4LogicalVolume* lead_source_logic =
	new G4LogicalVolume(lead_source_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb"), "LEAD_SOURCE");
      G4ThreeVector lead_source_pos =
	G4ThreeVector(lead_coll_pos.getX() + coll_thick/2. + source_thick/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, lead_source_pos),
                        lead_source_logic, 
			"LEAD_SOURCE", _shielding_air_logic, false, 0, false);
      

      G4LogicalVolume* cal_logic = _cal->GetLogicalVolume();
      source_pos =
	G4ThreeVector(lead_source_pos.getX() + source_thick/2. - _cal->GetCapsuleThickness()/2., lat_pos.getY(), lat_pos.getZ());
      new G4PVPlacement(G4Transform3D(*lat_rot, source_pos), cal_logic,
                        "SCREW_SUPPORT", _shielding_air_logic, false, 0, false);
      
      G4VisAttributes blue_col = nexus::Blue();
      G4VisAttributes yel_col = nexus::Yellow();
      G4VisAttributes red_col = nexus::Red();
      //  yel_col.SetForceSolid(true);
      //  blue_col.SetForceSolid(true);
      lead_coll_logic->SetVisAttributes(yel_col);
      lead_source_logic->SetVisAttributes(red_col);
      cal_logic->SetVisAttributes(blue_col);
     
  
      G4VisAttributes green_col = nexus::LightGreen();
      yel_col.SetForceSolid(true);
      blue_col.SetForceSolid(true);
      red_col.SetForceSolid(true);
      green_col.SetForceSolid(true);
      lead_out_freddy_logic->SetVisAttributes(green_col);
      lead_middle_freddy_logic->SetVisAttributes(blue_col);
      lead_screw_freddy_logic->SetVisAttributes(red_col);
      lead_nozzle_freddy_logic->SetVisAttributes(yel_col);

    }
     
     // Placement of the shielding volume, rotated and translated to have a right-handed ref system with z = z drift.
   
    _displ = G4ThreeVector(0., 0., _inner_elements->GetELzCoord());
    G4RotationMatrix rot;
    rot.rotateY(_rot_angle);
    new G4PVPlacement(G4Transform3D(rot, _displ), shielding_logic, "LEAD_BOX",
		      _lab_logic, false, 0, false);


    //// VERTEX GENERATORS   //
    _lab_gen = 
      new BoxPointSampler(_lab_size - 1.*m, _lab_size - 1.*m, _lab_size - 1.*m, 1.*m,G4ThreeVector(0.,0.,0.),0);

    // this is the position of the source inside the capsule
 
    G4ThreeVector gen_pos = source_pos - G4ThreeVector(_cal->GetSourceZpos(), 0., 0.);
    _lat_source_gen = new CylinderPointSampler(0., _cal->GetSourceThickness(), _cal->GetSourceDiameter()/2., 0., gen_pos, lat_rot);

    // G4double source_diam = na22->GetSourceDiameter();
    // G4double source_thick = na22->GetSourceThickness();
    // G4ThreeVector lat_pos_na22 =
    //   G4ThreeVector(lat_pos.getX() + na22->GetSourceThickness()/2., lat_pos.getY(), lat_pos.getZ());
    // G4ThreeVector up_pos_na22 =
    //   G4ThreeVector(up_pos.getX(), up_pos.getY() + na22->GetSourceThickness()/2., up_pos.getZ());

    // _source_gen_lat = new CylinderPointSampler(0., source_thick, source_diam/2., 0., lat_pos_na22, lat_rot);
    // _source_gen_up = new CylinderPointSampler(0., source_thick, source_diam/2., 0., up_pos_na22, up_rot);

    G4ThreeVector shielding_dim = _shielding->GetDimensions();

    _muon_gen =
      new MuonsPointSampler(shielding_dim.x()/2. + 50.*cm, shielding_dim.y()/2. + 1.*cm, shielding_dim.z()/2. + 50.*cm);

  }

 
    
  G4ThreeVector NextNew::GenerateVertex(const G4String& region) const
  {
    G4ThreeVector vertex(0.,0.,0.);
    //AIR AROUND SHIELDING
    if (region == "LAB") {
      vertex = _lab_gen->GenerateVertex("INSIDE");
    } else if (region == "MUONS") {
      vertex = _muon_gen->GenerateVertex();
    }
    else if (region == "EXTERNAL_PORT_ANODE") {
      vertex =  _lat_source_gen->GenerateVertex("BODY_VOL");
    }
    // else if (region == "NA22_PORT_UP_EXT") {
    //   vertex =  _source_gen_up->GenerateVertex("BODY_VOL");
    // }
    else if ( (region == "SHIELDING_LEAD") || (region == "SHIELDING_STEEL") || 
	      (region == "SHIELDING_GAS") || (region == "SHIELDING_STRUCT") ||
	      (region == "EXTERNAL") || (region == "SOURCE_PORT_AXIAL_EXT") ) {
      vertex = _shielding->GenerateVertex(region);   
    }
    //PEDESTAL
    else if (region == "PEDESTAL") {
      vertex = _pedestal->GenerateVertex(region);
    }
    //  //COPPER CASTLE
   // else if (region == "CU_CASTLE"){
   //   vertex = _cu_castle->GenerateVertex(region);
   //}
   //  //RADON 
   //  //on the inner lead surface (SHIELDING_GAS) and on the outer copper castle surface (RN_CU_CASTLE)
   // else if (region == "RN_CU_CASTLE") {
   //   vertex = _cu_castle->GenerateVertex(region);
   //  }

    //VESSEL REGIONS
    else if ( (region == "VESSEL") || 
	      (region == "SOURCE_PORT_ANODE") ||
	      (region == "SOURCE_PORT_UP") ||
	      (region == "SOURCE_PORT_AXIAL") ||
	      (region == "INTERNAL_PORT_ANODE") ){
      vertex = _vessel->GenerateVertex(region);
    }
    // ICS REGIONS
    else if (region == "ICS"){  
      vertex = _ics->GenerateVertex(region);
    }
    //INNER ELEMENTS
    else if ( (region == "CENTER") ||
	      (region == "CARRIER_PLATE") || (region == "ENCLOSURE_BODY") || (region == "ENCLOSURE_WINDOW") || 
	      (region == "OPTICAL_PAD") || (region == "PMT_BODY") || (region == "PMT_BASE") ||
	      (region == "DRIFT_TUBE") || (region == "BUFFER_TUBE") || (region == "ANODE_QUARTZ")||	     
	      (region == "ACTIVE") || (region == "EL_TABLE") || (region == "AD_HOC") || (region == "CATHODE")||
	      (region == "SUPPORT_PLATE") || (region == "DICE_BOARD") || (region == "DB_PLUG") ){
      vertex = _inner_elements->GenerateVertex(region);
    }
    else {
      G4Exception("[NextNew]", "GenerateVertex()", FatalException,
		  "Unknown vertex generation region!");     
    }

    // AD_HOC is the only vertex that is not rotated and shifted because it is passed by the user
    if  (region == "AD_HOC")
      return vertex;
    // First rotate, then shift
    vertex.rotate(_rot_angle, G4ThreeVector(0., 1., 0.));
    vertex = vertex + _displ;

    return vertex;
  }
  
  
} //end namespace nexus
