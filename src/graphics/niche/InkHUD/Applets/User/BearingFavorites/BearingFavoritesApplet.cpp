#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "./BearingFavoritesApplet.h"

#include "NodeDB.h"
#include "gps/GeoCoord.h"

using namespace NicheGraphics;

void InkHUD::BearingFavoritesApplet::onActivate()
{
    refreshFavorites();
    OSThread::enabled = true;
    OSThread::setIntervalFromNow(60 * 1000UL);
}

void InkHUD::BearingFavoritesApplet::onDeactivate()
{
    favorites.clear();
    seenTimestamps.clear();
    signalStrengths.clear();
    OSThread::disable();
}

int32_t InkHUD::BearingFavoritesApplet::runOnce()
{
    if (isActive())
        requestUpdate();
    return 60 * 1000UL;
}

// Rebuild the favorites list from NodeDB, computing bearing and distance for each
void InkHUD::BearingFavoritesApplet::refreshFavorites()
{
    favorites.clear();

    meshtastic_NodeInfoLite *ourNode = nodeDB->getMeshNode(nodeDB->getNodeNum());
    bool haveOurPosition = ourNode && nodeDB->hasValidPosition(ourNode);

    float ourLat = 0, ourLon = 0;
    if (haveOurPosition) {
        ourLat = ourNode->position.latitude_i * 1e-7;
        ourLon = ourNode->position.longitude_i * 1e-7;
    }

    for (int i = 0; i < nodeDB->getNumMeshNodes(); i++) {
        meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
        if (!node || node->num == nodeDB->getNodeNum())
            continue;
        if (!node->is_favorite)
            continue;

        FavoriteInfo info;
        info.nodeNum = node->num;

        if (haveOurPosition && nodeDB->hasValidPosition(node)) {
            float theirLat = node->position.latitude_i * 1e-7;
            float theirLon = node->position.longitude_i * 1e-7;

            info.distanceMeters = (int32_t)GeoCoord::latLongToMeter(theirLat, theirLon, ourLat, ourLon);

            float bearingRad = GeoCoord::bearing(ourLat, ourLon, theirLat, theirLon);
            info.bearingDegrees = GeoCoord::toDegrees(bearingRad);
            if (info.bearingDegrees < 0)
                info.bearingDegrees += 360.0f;

            auto tsIt = seenTimestamps.find(node->num);
            if (tsIt != seenTimestamps.end())
                info.lastSeenMillis = tsIt->second;

            auto sigIt = signalStrengths.find(node->num);
            if (sigIt != signalStrengths.end())
                info.signal = sigIt->second;

            info.hasPosition = true;
        }

        favorites.push_back(info);
    }

    // Nodes with known position first (sorted by distance), then unknowns
    std::sort(favorites.begin(), favorites.end(), [](const FavoriteInfo &a, const FavoriteInfo &b) {
        if (a.hasPosition != b.hasPosition)
            return a.hasPosition;
        if (a.hasPosition && b.hasPosition)
            return a.distanceMeters < b.distanceMeters;
        return false;
    });
}

ProcessMessage InkHUD::BearingFavoritesApplet::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (!isActive())
        return ProcessMessage::CONTINUE;

    bool relevant = false;

    if (isFromUs(&mp)) {
        relevant = true;
    } else {
        const meshtastic_NodeInfoLite *sender = nodeDB->getMeshNode(mp.from);
        if (sender && sender->is_favorite)
            relevant = true;
    }

    if (!relevant)
        return ProcessMessage::CONTINUE;

    // Record when we last saw a position from this node, and its signal
    if (!isFromUs(&mp)) {
        seenTimestamps[mp.from] = millis();
        signalStrengths[mp.from] = getSignalStrength(mp.rx_snr, mp.rx_rssi);
    }

    // Snapshot old state to detect meaningful changes
    std::vector<FavoriteInfo> oldFavorites = favorites;
    refreshFavorites();

    bool changed = (oldFavorites.size() != favorites.size());
    if (!changed) {
        for (size_t i = 0; i < favorites.size(); i++) {
            float bearingDelta = oldFavorites[i].bearingDegrees - favorites[i].bearingDegrees;
            if (bearingDelta < 0)
                bearingDelta = -bearingDelta;

            if (oldFavorites[i].nodeNum != favorites[i].nodeNum ||
                oldFavorites[i].distanceMeters != favorites[i].distanceMeters ||
                oldFavorites[i].hasPosition != favorites[i].hasPosition || bearingDelta > 1.0f) {
                changed = true;
                break;
            }
        }
    }

    if (changed) {
        requestAutoshow();
        requestUpdate();
    }

    return ProcessMessage::CONTINUE;
}

// Draw a small compass arrow inside a circle, pointing toward the given bearing
void InkHUD::BearingFavoritesApplet::drawBearingArrow(int16_t cx, int16_t cy, int16_t radius, float bearingDeg)
{
    drawCircle(cx, cy, radius, BLACK);

    // Screen coordinates: 0° bearing = north = up (-Y), 90° = east = right (+X)
    float rad = bearingDeg * PI / 180.0f;

    // Tip of arrow at edge of circle
    int16_t tipX = cx + (int16_t)(radius * 0.85f * sinf(rad));
    int16_t tipY = cy - (int16_t)(radius * 0.85f * cosf(rad));

    // Two base points spread behind the tip
    float spread = 2.5f;
    int16_t base1X = cx + (int16_t)(radius * 0.35f * sinf(rad + spread));
    int16_t base1Y = cy - (int16_t)(radius * 0.35f * cosf(rad + spread));
    int16_t base2X = cx + (int16_t)(radius * 0.35f * sinf(rad - spread));
    int16_t base2Y = cy - (int16_t)(radius * 0.35f * cosf(rad - spread));

    fillTriangle(tipX, tipY, base1X, base1Y, base2X, base2Y, BLACK);
}

void InkHUD::BearingFavoritesApplet::drawSignalIndicator(int16_t x, int16_t y, uint16_t w, uint16_t h,
                                                         SignalStrength strength)
{
    constexpr float paddingW = 0.1;
    constexpr float paddingH = 0.1;
    constexpr float gutterW = 0.1;
    constexpr float barHRel[] = {0.3, 0.5, 0.7, 1.0};
    constexpr uint8_t barCount = 4;

    float barW = (1.0 - (paddingW + ((barCount - 1) * gutterW) + paddingW)) / barCount;
    float barHMax = 1.0 - (paddingH + paddingH);

    for (uint8_t i = 0; i < barCount; i++) {
        float barH = barHMax * barHRel[i];
        float barX = paddingW + (i * (gutterW + barW));
        float barY = paddingH + (barHMax - barH);

        int16_t rX = (x + (w * barX)) + 0.5;
        int16_t rY = (y + (h * barY)) + 0.5;
        uint16_t rW = (w * barW) + 0.5;
        uint16_t rH = (h * barH) + 0.5;

        if (i <= strength)
            drawRect(rX, rY, rW, rH, BLACK);
        else {
            float lineY = barY + barH;
            uint16_t rLineY = (y + (h * lineY)) + 0.5;
            drawLine(rX, rLineY, rX + rW - 1, rLineY, BLACK);
        }
    }
}

std::string InkHUD::BearingFavoritesApplet::formatAge(uint32_t seenAtMillis)
{
    if (seenAtMillis == 0)
        return "";

    uint32_t ageSecs = (millis() - seenAtMillis) / 1000;

    if (ageSecs < 120)
        return "just now";
    if (ageSecs < 3600)
        return to_string(ageSecs / 60) + "min ago";
    if (ageSecs < 86400)
        return to_string(ageSecs / 3600) + "h ago";
    return to_string(ageSecs / 86400) + "d ago";
}

void InkHUD::BearingFavoritesApplet::onRender(bool full)
{
    // Header
    std::string headerText = "Node Locator: ";
    headerText += to_string(favorites.size());
    headerText += (favorites.size() == 1) ? " node" : " nodes";
    drawHeader(headerText);

    int16_t headerDivY = getHeaderHeight() - 1;
    constexpr uint16_t padDivH = 2;

    // Empty state: no favorites at all
    if (favorites.empty()) {
        setFont(fontSmall);
        printAt(X(0.5), Y(0.5) - fontSmall.lineHeight(), "Mark nodes as favorites", CENTER, MIDDLE);
        printAt(X(0.5), Y(0.5), "to see bearing and", CENTER, MIDDLE);
        printAt(X(0.5), Y(0.5) + fontSmall.lineHeight(), "distance here", CENTER, MIDDLE);
        return;
    }

    // Empty state: we have no GPS fix yet
    meshtastic_NodeInfoLite *ourNode = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (!ourNode || !nodeDB->hasValidPosition(ourNode)) {
        setFont(fontMedium);
        printAt(X(0.5), Y(0.5) - (fontMedium.lineHeight() / 2), "Waiting for", CENTER, MIDDLE);
        printAt(X(0.5), Y(0.5) + (fontMedium.lineHeight() / 2), "GPS fix", CENTER, MIDDLE);
        return;
    }

    int16_t arrowRadius = fontMedium.lineHeight() * 0.45;
    uint16_t cardTopY = headerDivY + padDivH;

    for (auto &fav : favorites) {
        if (cardTopY + cardH > height())
            break;

        meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(fav.nodeNum);
        if (!node)
            continue;

        // Three-line card layout
        uint16_t lineAY = cardTopY + (fontLarge.lineHeight() / 2);
        uint16_t lineBY = cardTopY + fontLarge.lineHeight() + (fontMedium.lineHeight() / 2);
        uint16_t lineCY = cardTopY + fontLarge.lineHeight() + fontMedium.lineHeight() + (fontMedium.lineHeight() / 2);

        // -- Short name (left, line A) --
        std::string shortName = parseShortName(node);
        setFont(fontLarge);
        printAt(0, lineAY, shortName, LEFT, MIDDLE);

        // -- Signal indicator (after short name, line A) --
        if (fav.signal != SignalStrength::SIGNAL_UNKNOWN) {
            uint16_t sigH = fontMedium.lineHeight();
            uint16_t sigW = sigH * 1.5;
            int16_t sigX = getTextWidth(shortName) + 4;
            int16_t sigY = lineAY - (sigH / 2);
            drawSignalIndicator(sigX, sigY, sigW, sigH, fav.signal);
        }

        // -- Age (right, line A) -- always shown, placeholder if unavailable --
        std::string age = formatAge(fav.lastSeenMillis);
        if (age.empty())
            age = "-- ago";
        setFont(fontMedium);
        printAt(width() - 1, lineAY, age, RIGHT, MIDDLE);

        if (fav.hasPosition) {
            // -- Compass arrow (far right, line B) --
            int16_t arrowCX = width() - arrowRadius - 1;
            drawBearingArrow(arrowCX, lineBY, arrowRadius, fav.bearingDegrees);

            // -- Bearing text to the left of arrow (line B) --
            unsigned int degInt = ((unsigned int)(fav.bearingDegrees + 0.5f)) % 360;
            std::string bearingText = GeoCoord::degreesToBearing(degInt);
            bearingText += " ";
            bearingText += to_string(degInt);
            bearingText += "\xB0T"; // degree symbol (WIN1252) + T for true bearing

            setFont(fontMedium);
            printAt(arrowCX - arrowRadius - 3, lineBY, bearingText, RIGHT, MIDDLE);

            // -- Distance (left, line B) --
            std::string distance = localizeDistance(fav.distanceMeters);
            printAt(0, lineBY, distance, LEFT, MIDDLE);

            // -- GPS coordinates with hemisphere (left, line C) --
            float lat = node->position.latitude_i * 1e-7f;
            float lon = node->position.longitude_i * 1e-7f;
            char latH = (lat >= 0) ? 'N' : 'S';
            char lonH = (lon >= 0) ? 'E' : 'W';
            if (lat < 0) lat = -lat;
            if (lon < 0) lon = -lon;
            char coordStr[36];
            snprintf(coordStr, sizeof(coordStr), "%.5f%c, %.5f%c", lat, latH, lon, lonH);
            setFont(fontMedium);
            printAt(0, lineCY, coordStr, LEFT, MIDDLE);
        } else {
            setFont(fontMedium);
            printAt(width() - 1, lineBY, "No position", RIGHT, MIDDLE);
        }

        cardTopY += cardH;
    }
}

#endif
