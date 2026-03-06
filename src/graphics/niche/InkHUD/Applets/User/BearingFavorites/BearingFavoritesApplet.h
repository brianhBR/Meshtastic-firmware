#ifdef MESHTASTIC_INCLUDE_INKHUD

/*

Node Locator: shows bearing, distance, and GPS coordinates for each
favorited node. Designed for locating remote devices such as drop cameras, beacons, etc.

Displays absolute bearing (compass direction) since InkHUD builds
do not have access to device heading / magnetometer data.

    +-------------------------------+
    |                               |
    |  SHRT  [||||]     5min ago    |
    |  1.2 km   NE 45°  (arrow)    |
    |  35.1234, -120.5678           |
    |                               |
    |  CAM2  [||||]    12min ago    |
    |  3.5 km   SSW 210° (arrow)   |
    |  34.9876, -119.4321           |
    |                               |
    +-------------------------------+

*/

#pragma once

#include "configuration.h"

#include <map>

#include "graphics/niche/InkHUD/Applet.h"

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

namespace NicheGraphics::InkHUD
{

class BearingFavoritesApplet : public Applet, public SinglePortModule, public concurrency::OSThread
{
  public:
    BearingFavoritesApplet()
        : SinglePortModule("BearingFavoritesApplet", meshtastic_PortNum_POSITION_APP),
          concurrency::OSThread("BearingFavoritesApplet")
    {
        OSThread::disable();
    }
    void onRender(bool full) override;
    void onActivate() override;
    void onDeactivate() override;

  protected:
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    struct FavoriteInfo {
        static constexpr int32_t DISTANCE_UNKNOWN = -1;

        NodeNum nodeNum = 0;
        int32_t distanceMeters = DISTANCE_UNKNOWN;
        float bearingDegrees = 0;
        bool hasPosition = false;
        uint32_t lastSeenMillis = 0;
        SignalStrength signal = SignalStrength::SIGNAL_UNKNOWN;
    };

    void refreshFavorites();
    void drawBearingArrow(int16_t cx, int16_t cy, int16_t radius, float bearingDeg);
    void drawSignalIndicator(int16_t x, int16_t y, uint16_t w, uint16_t h, SignalStrength strength);
    std::string formatAge(uint32_t seenAtMillis);

    std::vector<FavoriteInfo> favorites;
    std::map<NodeNum, uint32_t> seenTimestamps;
    std::map<NodeNum, SignalStrength> signalStrengths;

    uint8_t cardMarginH = fontSmall.lineHeight();
    uint16_t cardH = fontLarge.lineHeight() + fontMedium.lineHeight() + fontMedium.lineHeight() + cardMarginH;
};

} // namespace NicheGraphics::InkHUD

#endif
