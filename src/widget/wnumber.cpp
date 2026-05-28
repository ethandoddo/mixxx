#include "widget/wnumber.h"

#include "moc_wnumber.cpp"
#include "skin/legacy/skincontext.h"

WNumber::WNumber(QWidget* pParent, int numberOfDigits)
        : WLabel(pParent),
                    m_iNoDigits(numberOfDigits) {
}

void WNumber::setup(const QDomNode& node, const SkinContext& context) {
    WLabel::setup(node, context);

    // Number of digits after the decimal.
    context.hasNodeSelectInt(node, "NumberOfDigits", &m_iNoDigits);

    setValue(0.);
}

void WNumber::onConnectedControlChanged(double dParameter, double dValue) {
    Q_UNUSED(dParameter);
    // We show the actual control value instead of its parameter.
    setValue(dValue);
}

void WNumber::setValue(double dValue) {
    const QString newText = m_skinText.contains("%1")
            ? m_skinText.arg(QString::number(dValue, 'f', m_iNoDigits))
            : m_skinText + QString::number(dValue, 'f', m_iNoDigits);
    if (newText == m_lastDisplayText) {
        return;
    }
    m_lastDisplayText = newText;
    setText(newText);
}
