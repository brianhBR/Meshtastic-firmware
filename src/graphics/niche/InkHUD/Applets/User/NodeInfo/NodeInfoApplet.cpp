#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "./NodeInfoApplet.h"

#include "NodeDB.h"
#include "gps/GeoCoord.h"
#include "main.h" // owner, powerStatus

#include <cstdio>

using namespace NicheGraphics;

InkHUD::NodeInfoApplet::NodeInfoApplet()
{
    // Battery info -- get notified when the rounded SoC changes
    powerStatusObserver.observe(&powerStatus->onNewStatus);
}

int InkHUD::NodeInfoApplet::onPowerStatusUpdate(const meshtastic::Status *status)
{
    if (!isActive())
        return 0;

    if (status->getStatusType() != STATUS_TYPE_POWER)
        return 0;

    const meshtastic::PowerStatus *p = (const meshtastic::PowerStatus *)status;

    // Round to the nearest 10% to keep e-ink rewrites infrequent
    uint8_t soc = ((p->getBatteryChargePercent() + 5) / 10) * 10;
    if (soc != lastSocRounded) {
        lastSocRounded = soc;
        requestUpdate();
    }
    return 0;
}

void InkHUD::NodeInfoApplet::onRender(bool full)
{
    drawHeader("Node Info");
    const uint16_t headerH = getHeaderHeight();

    // Vertical layout: stacked rows of small text, generous line spacing for readability.
    setFont(fontSmall);
    const int16_t lineH = static_cast<int16_t>(fontSmall.lineHeight() * 1.4f);
    int16_t cursorY = headerH + (lineH / 4);

    auto drawRow = [&](const std::string &text) {
        printAt(0, cursorY, text);
        cursorY += lineH;
    };

    // ---------------------------------------------------------------
    // Owner identity
    // ---------------------------------------------------------------
    char idBuf[12];
    snprintf(idBuf, sizeof(idBuf), "%08x", (unsigned)nodeDB->getNodeNum());

    std::string nameLine = std::string(owner.long_name);
    if (owner.short_name[0] != '\0') {
        nameLine += " (";
        nameLine += owner.short_name;
        nameLine += ")";
    }

    // Use the medium font for the name -- it's the most important line on the screen.
    setFont(fontMedium);
    printAt(0, cursorY, nameLine);
    cursorY += static_cast<int16_t>(fontMedium.lineHeight() * 1.2f);
    setFont(fontSmall);

    drawRow(std::string("ID: !") + idBuf);

    // ---------------------------------------------------------------
    // Firmware version
    // ---------------------------------------------------------------
    drawRow(std::string("FW: ") + xstr(APP_VERSION));

    // ---------------------------------------------------------------
    // Battery
    // ---------------------------------------------------------------
    if (powerStatus && powerStatus->getHasBattery()) {
        uint16_t mv = powerStatus->getBatteryVoltageMv();
        uint8_t pct = powerStatus->getBatteryChargePercent();
        char batBuf[48];
        const char *suffix = powerStatus->getHasUSB() ? (powerStatus->getIsCharging() ? " USB+chg" : " USB") : "";
        snprintf(batBuf, sizeof(batBuf), "Batt: %u.%02uV  %u%%%s", mv / 1000u, (mv % 1000u) / 10u, pct, suffix);
        drawRow(batBuf);
    } else if (powerStatus && powerStatus->getHasUSB()) {
        drawRow("Batt: USB powered");
    } else {
        drawRow("Batt: --");
    }

    // ---------------------------------------------------------------
    // Local position
    // ---------------------------------------------------------------
    meshtastic_NodeInfoLite *us = nodeDB->getMeshNode(nodeDB->getNodeNum());
    bool hasPos = (us != nullptr) && nodeDB->hasValidPosition(us);

    if (hasPos) {
        const float lat = us->position.latitude_i * 1e-7f;
        const float lng = us->position.longitude_i * 1e-7f;
        char latBuf[24];
        char lngBuf[24];
        snprintf(latBuf, sizeof(latBuf), "Lat: %+.5f", lat);
        snprintf(lngBuf, sizeof(lngBuf), "Lon: %+.5f", lng);
        drawRow(latBuf);
        drawRow(lngBuf);

        if (us->position.altitude != 0) {
            char altBuf[24];
            snprintf(altBuf, sizeof(altBuf), "Alt: %dm", (int)us->position.altitude);
            drawRow(altBuf);
        }

        // Cache for change detection (used by external triggers if/when added)
        lastLatI = us->position.latitude_i;
        lastLngI = us->position.longitude_i;
        lastHadPosition = true;
    } else {
        drawRow("GPS: no fix");
        lastHadPosition = false;
    }
}

#endif
