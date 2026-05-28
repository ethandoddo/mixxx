#include "waveform/renderers/allshader/waveformrenderbeat.h"

#include <QDomNode>

#include "moc_waveformrenderbeat.cpp"
#include "rendergraph/geometry.h"
#include "rendergraph/material/unicolormaterial.h"
#include "rendergraph/vertexupdaters/vertexupdater.h"
#include "skin/legacy/skincontext.h"
#include "track/cue.h"
#include "track/track.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

using namespace rendergraph;

namespace allshader {

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type,
        bool downbeatOnly)
        : ::WaveformRendererAbstract(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip),
          m_downbeatOnly(downbeatOnly) {
    initForRectangles<UniColorMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& skinContext) {
    if (m_downbeatOnly) {
        m_color = QColor(220, 30, 30);
    } else {
        m_color = QColor(skinContext.selectString(node, QStringLiteral("BeatColor")));
        m_color = WSkinColor::getCorrectColor(m_color).toRgb();
    }
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* event) {
    Q_UNUSED(painter);
    Q_UNUSED(event);
    DEBUG_ASSERT(false);
}

void WaveformRenderBeat::preprocess() {
    if (!preprocessInner()) {
        geometry().allocate(0);
        markDirtyGeometry();
    }
}

bool WaveformRenderBeat::preprocessInner() {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();

    if (!trackInfo || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    mixxx::BeatsPointer trackBeats = trackInfo->getBeats();
    if (!trackBeats) {
        return false;
    }

#ifndef __SCENEGRAPH__
    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return false;
    }
    m_color.setAlphaF(alpha / 100.0f);
#endif

    if (!m_color.alpha()) {
        // Don't render the beatgrid lines is there are fully transparent
        return true;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0.0) {
        return false;
    }

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition(positionType);
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition(positionType);

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);

    if (!startPosition.isValid() || !endPosition.isValid()) {
        return false;
    }

    const float rendererBreadth = m_waveformRenderer->getBreadth();

    const int numVerticesPerLine = 6; // 2 triangles

    QList<CuePointer> cuePoints;
    if (m_downbeatOnly) {
        cuePoints = trackInfo->getCuePoints();
    }

    mixxx::audio::FramePos anchorBeatPos;
    if (m_downbeatOnly) {
        for (const CuePointer& pCue : cuePoints) {
            if (pCue->getType() == mixxx::CueType::HotCue &&
                    pCue->getHotCue() == mixxx::kFirstHotCueIndex) {
                mixxx::audio::FramePos cuePos = pCue->getPosition();
                if (cuePos.isValid()) {
                    anchorBeatPos = trackBeats->findClosestBeat(cuePos);
                }
                break;
            }
        }
    }

    if (m_downbeatOnly && !anchorBeatPos.isValid()) {
        auto firstIt = trackBeats->cbegin();
        if (firstIt != trackBeats->cend()) {
            anchorBeatPos = *firstIt;
        }
    }

    int numBeatsInRange = 0;
    if (m_downbeatOnly) {
        auto it = trackBeats->iteratorFrom(startPosition);
        int beatIndex = 0;
        if (anchorBeatPos.isValid()) {
            auto anchorIt = trackBeats->iteratorFrom(anchorBeatPos);
            if (anchorIt != trackBeats->cend() && it != trackBeats->cend()) {
                beatIndex = static_cast<int>(it - anchorIt);
            }
        }
        for (; it != trackBeats->cend() && *it <= endPosition; ++it, ++beatIndex) {
            bool isDownbeat = (((beatIndex % 4) + 4) % 4 == 0);
            if (isDownbeat) {
                numBeatsInRange++;
            }
        }
    } else {
        for (auto it = trackBeats->iteratorFrom(startPosition);
                it != trackBeats->cend() && *it <= endPosition;
                ++it) {
            numBeatsInRange++;
        }
    }

    const int reserved = numBeatsInRange * numVerticesPerLine;
    geometry().allocate(reserved);

    VertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::Point2D>()};

    int beatIndex = 0;
    if (m_downbeatOnly && anchorBeatPos.isValid()) {
        auto anchorIt = trackBeats->iteratorFrom(anchorBeatPos);
        auto firstIt = trackBeats->iteratorFrom(startPosition);
        if (anchorIt != trackBeats->cend() && firstIt != trackBeats->cend()) {
            beatIndex = static_cast<int>(firstIt - anchorIt);
        }
    }

    for (auto it = trackBeats->iteratorFrom(startPosition);
            it != trackBeats->cend() && *it <= endPosition;
            ++it) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(
                        beatPosition, positionType);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        const float x1 = static_cast<float>(xBeatPoint);
        const float x2 = x1 + 1.f;

        if (m_downbeatOnly) {
            bool isDownbeat = (((beatIndex % 4) + 4) % 4 == 0);
            if (isDownbeat) {
                vertexUpdater.addRectangle({x1, 0.f},
                        {x2, m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth});
            }
            ++beatIndex;
        } else {
            vertexUpdater.addRectangle({x1, 0.f},
                    {x2, m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth});
        }
    }
    markDirtyGeometry();

    DEBUG_ASSERT(vertexUpdater.index() <= reserved);

    material().setUniform(1, m_color);
    markDirtyMaterial();

    return true;
}

} // namespace allshader
