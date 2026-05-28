#include "waveform/renderers/waveformrenderbeat.h"

#include <QPainter>

#include "track/cue.h"
#include "track/track.h"
#include "util/painterscope.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

class QPaintEvent;

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidgetRenderer)
        : WaveformRendererAbstract(waveformWidgetRenderer) {
    m_beats.resize(128);
}

WaveformRenderBeat::~WaveformRenderBeat() {
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& context) {
    m_beatColor = QColor(context.selectString(node, "BeatColor"));
    m_beatColor = WSkinColor::getCorrectColor(m_beatColor).toRgb();
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* /*event*/) {
    TrackPointer pTrackInfo = m_waveformRenderer->getTrackInfo();

    if (!pTrackInfo) {
        return;
    }

    mixxx::BeatsPointer trackBeats = pTrackInfo->getBeats();
    if (!trackBeats) {
        return;
    }

    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return;
    }
#ifdef MIXXX_USE_QOPENGL
    // Using alpha transparency with drawLines causes a graphical issue when
    // drawing with QPainter on the QOpenGLWindow: instead of individual lines
    // a large rectangle encompassing all beatlines is drawn.
    m_beatColor.setAlphaF(1.f);
#else
    m_beatColor.setAlphaF(alpha/100.0);
#endif

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition();
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition();

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);
    auto it = trackBeats->iteratorFrom(startPosition);

    // if no beat do not waste time saving/restoring painter
    if (it == trackBeats->cend() || *it > endPosition) {
        return;
    }

    PainterScope PainterScope(painter);

    painter->setRenderHint(QPainter::Antialiasing);

    QPen beatPen(m_beatColor);
    beatPen.setWidthF(std::max(1.0, scaleFactor()));
    painter->setPen(beatPen);

    const Qt::Orientation orientation = m_waveformRenderer->getOrientation();
    const float rendererWidth = m_waveformRenderer->getWidth();
    const float rendererHeight = m_waveformRenderer->getHeight();

    QVector<QLineF> downbeats;
    downbeats.resize(128);
    int beatCount = 0;
    int downbeatCount = 0;

    // Find the A cue (hot cue index 0) to use as the downbeat anchor.
    // Fall back to the first beat of the track if no A cue is set.
    mixxx::audio::FramePos anchorBeatPos;
    const QList<CuePointer> cuePoints = pTrackInfo->getCuePoints();
    for (const CuePointer& pCue : cuePoints) {
        if (pCue->getType() == mixxx::CueType::HotCue &&
                pCue->getHotCue() == mixxx::kFirstHotCueIndex) {
            mixxx::audio::FramePos cuePos = pCue->getPosition();
            if (cuePos.isValid()) {
                // Snap to the closest beat so we're always on a beat boundary.
                // This is sample-accurate — no floating point BPM math.
                anchorBeatPos = trackBeats->findClosestBeat(cuePos);
            }
            break;
        }
    }

    // No A cue — fall back to the first beat of the track
    if (!anchorBeatPos.isValid()) {
        auto firstIt = trackBeats->cbegin();
        if (firstIt != trackBeats->cend()) {
            anchorBeatPos = *firstIt;
        }
    }

    int beatIndex = 0;
    if (anchorBeatPos.isValid()) {
        auto anchorIt = trackBeats->iteratorFrom(anchorBeatPos);
        if (anchorIt != trackBeats->cend()) {
            beatIndex = it - anchorIt;
        }
    }

    for (; it != trackBeats->cend() && *it <= endPosition; ++it, ++beatIndex) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(beatPosition);
        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        // beatIndex == 0 is the anchor (A cue beat).
        // Every 4th beat from there is a downbeat.
        // Positive modulo handles negative indices (beats before anchor) correctly.
        bool isDownbeat = (((beatIndex % 4) + 4) % 4 == 0);

        if (isDownbeat) {
            if (downbeatCount >= downbeats.size()) {
                downbeats.resize(downbeats.size() * 2);
            }
            if (orientation == Qt::Horizontal) {
                downbeats[downbeatCount++].setLine(xBeatPoint, 0.0f, xBeatPoint, rendererHeight);
            } else {
                downbeats[downbeatCount++].setLine(0.0f, xBeatPoint, rendererWidth, xBeatPoint);
            }
        } else {
            if (beatCount >= m_beats.size()) {
                m_beats.resize(m_beats.size() * 2);
            }
            if (orientation == Qt::Horizontal) {
                m_beats[beatCount++].setLine(xBeatPoint, 0.0f, xBeatPoint, rendererHeight);
            } else {
                m_beats[beatCount++].setLine(0.0f, xBeatPoint, rendererWidth, xBeatPoint);
            }
        }
    }

    // Draw regular beats (existing color from skin)
    painter->drawLines(m_beats.constData(), beatCount);

    // Draw downbeats in red (anchored to A cue, every 4 beats)
    QColor downbeatColor(220, 30, 30);
    downbeatColor.setAlphaF(m_beatColor.alphaF());
    QPen downbeatPen(downbeatColor);
    downbeatPen.setWidthF(std::max(1.0, scaleFactor()));
    painter->setPen(downbeatPen);
    painter->drawLines(downbeats.constData(), downbeatCount);
}
