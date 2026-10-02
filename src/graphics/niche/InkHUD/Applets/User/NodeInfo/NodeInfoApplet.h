#ifdef MESHTASTIC_INCLUDE_INKHUD

/*

Single-screen "About this node" applet.

Renders a static layout summarising the local node:
  - Owner long name + short name + node id (hex)
  - Firmware version
  - Battery voltage / state-of-charge / charge state
  - Local GPS position (lat/lon) or "No fix"

Re-renders are triggered conservatively (e-ink friendly):
  - When rounded battery state-of-charge changes by 10%
  - When the local node's position moves more than ~25m

*/

#pragma once

#include "configuration.h"

#include "graphics/niche/InkHUD/Applet.h"

#include "PowerStatus.h"

namespace NicheGraphics::InkHUD
{

class NodeInfoApplet : public Applet
{
  public:
    NodeInfoApplet();

    void onRender(bool full) override;

  private:
    int onPowerStatusUpdate(const meshtastic::Status *status);

    CallbackObserver<NodeInfoApplet, const meshtastic::Status *> powerStatusObserver =
        CallbackObserver<NodeInfoApplet, const meshtastic::Status *>(this, &NodeInfoApplet::onPowerStatusUpdate);

    uint8_t lastSocRounded = 0;     // Battery state of charge, rounded to nearest 10% (for change detection)
    int32_t lastLatI = 0;           // Last rendered position, Meshtastic int32 format (1e-7 deg)
    int32_t lastLngI = 0;
    bool lastHadPosition = false;
};

} // namespace NicheGraphics::InkHUD

#endif
