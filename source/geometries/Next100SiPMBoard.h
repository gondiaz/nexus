// -----------------------------------------------------------------------------
//  nexus | Next100SiPMBoard.h
//
//  * Author: <justo.martin-albo@ific.uv.es>
//  * Creation date: 7 January 2020
// -----------------------------------------------------------------------------

#ifndef NEXT100_SIPM_BOARD_H
#define NEXT100_SIPM_BOARD_H

#include "BaseGeometry.h"
#include <G4ThreeVector.hh>
#include <vector>

class G4VPhysicalVolume;


namespace nexus {

  class BoxPointSampler;

  // Geometry of the 8x8 SiPM boards used in the tracking plane of NEXT-100

  class Next100SiPMBoard: public nexus::BaseGeometry
  {
  public:
    // Default constructor
    Next100SiPMBoard();
    // Destructor
    ~Next100SiPMBoard();
    //
    void SetMotherPhysicalVolume(G4VPhysicalVolume*);
    //
    void Construct();
    //
    G4ThreeVector GenerateVertex(const G4String&) const;

    G4double GetSize() const;
    G4double GetThickness() const;

  private:
    G4double size_, thickness_, pitch_, margin_;
    std::vector<G4ThreeVector> sipm_positions;
    G4VPhysicalVolume* mpv_;
    BoxPointSampler* vtxgen_;
  };

  inline void     Next100SiPMBoard::SetMotherPhysicalVolume(G4VPhysicalVolume* p)
  { mpv_ = p;}
  inline G4double Next100SiPMBoard::GetSize() const
  { return size_; }
  inline G4double Next100SiPMBoard::GetThickness() const
  { return thickness_; }

} // namespace nexus

#endif
