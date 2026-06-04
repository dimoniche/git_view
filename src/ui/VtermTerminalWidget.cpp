#include "ui/VtermTerminalWidget.h"

#include "ui/PtySession.h"

#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

#include <vterm.h>

namespace {

VTermModifier qtModifiersToVterm(Qt::KeyboardModifiers modifiers)
{
    int mod = VTERM_MOD_NONE;
    if (modifiers & Qt::ShiftModifier) {
        mod |= VTERM_MOD_SHIFT;
    }
    if (modifiers & Qt::AltModifier) {
        mod |= VTERM_MOD_ALT;
    }
    if (modifiers & Qt::ControlModifier) {
        mod |= VTERM_MOD_CTRL;
    }
    return static_cast<VTermModifier>(mod);
}

VTermKey qtKeyToVterm(int key)
{
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VTERM_KEY_ENTER;
    case Qt::Key_Tab:
        return VTERM_KEY_TAB;
    case Qt::Key_Backspace:
        return VTERM_KEY_BACKSPACE;
    case Qt::Key_Escape:
        return VTERM_KEY_ESCAPE;
    case Qt::Key_Up:
        return VTERM_KEY_UP;
    case Qt::Key_Down:
        return VTERM_KEY_DOWN;
    case Qt::Key_Left:
        return VTERM_KEY_LEFT;
    case Qt::Key_Right:
        return VTERM_KEY_RIGHT;
    case Qt::Key_Home:
        return VTERM_KEY_HOME;
    case Qt::Key_End:
        return VTERM_KEY_END;
    case Qt::Key_PageUp:
        return VTERM_KEY_PAGEUP;
    case Qt::Key_PageDown:
        return VTERM_KEY_PAGEDOWN;
    case Qt::Key_Insert:
        return VTERM_KEY_INS;
    case Qt::Key_Delete:
        return VTERM_KEY_DEL;
    case Qt::Key_F1:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(0));
    case Qt::Key_F2:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(1));
    case Qt::Key_F3:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(2));
    case Qt::Key_F4:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(3));
    case Qt::Key_F5:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(4));
    case Qt::Key_F6:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(5));
    case Qt::Key_F7:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(6));
    case Qt::Key_F8:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(7));
    case Qt::Key_F9:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(8));
    case Qt::Key_F10:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(9));
    case Qt::Key_F11:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(10));
    case Qt::Key_F12:
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(11));
    default:
        return VTERM_KEY_NONE;
    }
}

QColor colorFromVterm(const VTermColor *color, bool isForeground, const QColor &defaultColor)
{
    if (!color) {
        return defaultColor;
    }
    if (VTERM_COLOR_IS_RGB(color)) {
        return QColor(color->rgb.red, color->rgb.green, color->rgb.blue);
    }
    if (VTERM_COLOR_IS_DEFAULT_FG(color) || VTERM_COLOR_IS_DEFAULT_BG(color)) {
        return defaultColor;
    }
    static const QColor palette[16] = {
        QColor(0x1e, 0x1e, 0x1e), QColor(0xcd, 0x31, 0x31), QColor(0x0d, 0x93, 0x0d),
        QColor(0xe5, 0xe5, 0x0d), QColor(0x3d, 0x6f, 0xd8), QColor(0x9d, 0x3a, 0xda),
        QColor(0x2a, 0xa7, 0xa7), QColor(0xd3, 0xd3, 0xd3), QColor(0x4e, 0x4e, 0x4e),
        QColor(0xff, 0x55, 0x55), QColor(0x55, 0xff, 0x55), QColor(0xff, 0xff, 0x55),
        QColor(0x55, 0x55, 0xff), QColor(0xff, 0x55, 0xff), QColor(0x55, 0xff, 0xff),
        QColor(0xff, 0xff, 0xff),
    };
    const int index = color->indexed.idx & 15;
    Q_UNUSED(isForeground);
    return palette[index];
}

void outputCallback(const char *s, size_t len, void *user)
{
    auto *pty = static_cast<PtySession *>(user);
    pty->write(QByteArray(s, static_cast<int>(len)));
}

int damageCallback(VTermRect rect, void *user)
{
    Q_UNUSED(rect);
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    widget->update();
    return 1;
}

int movecursorCallback(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
    Q_UNUSED(pos);
    Q_UNUSED(oldpos);
    Q_UNUSED(visible);
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    widget->update();
    return 1;
}

int settermpropCallback(VTermProp prop, VTermValue *val, void *user)
{
    Q_UNUSED(prop);
    Q_UNUSED(val);
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    widget->update();
    return 1;
}

int bellCallback(void *user)
{
    Q_UNUSED(user);
    return 1;
}

int resizeCallback(int rows, int cols, void *user)
{
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    widget->setGridSize(rows, cols);
    return 1;
}

} // namespace

VtermTerminalWidget::VtermTerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    setPalette(pal);

    QFont font = QFont(QStringLiteral("Menlo"));
#if defined(Q_OS_WIN)
    font = QFont(QStringLiteral("Consolas"));
#elif defined(Q_OS_LINUX)
    font = QFont(QStringLiteral("Monospace"));
#endif
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(11);
    setFont(font);

    const QFontMetrics metrics(font);
    m_cellWidth = qMax(7, metrics.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = qMax(12, metrics.height());

    m_pty = new PtySession(this);
    connect(m_pty, &PtySession::readyRead, this, &VtermTerminalWidget::onPtyReadyRead);
    connect(m_pty, &PtySession::exited, this, &VtermTerminalWidget::onPtyExited);
}

VtermTerminalWidget::~VtermTerminalWidget()
{
    stopShell();
}

bool VtermTerminalWidget::startShell(const QString &workingDirectory)
{
    stopShell();

    updateTerminalSize();

    m_vterm = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vterm, 1);
    vterm_output_set_callback(m_vterm, outputCallback, m_pty);

    m_vtermScreen = vterm_obtain_screen(m_vterm);
    static VTermScreenCallbacks screenCallbacks = {};
    screenCallbacks.damage = damageCallback;
    screenCallbacks.movecursor = movecursorCallback;
    screenCallbacks.settermprop = settermpropCallback;
    screenCallbacks.bell = bellCallback;
    screenCallbacks.resize = resizeCallback;
    screenCallbacks.sb_pushline = onSbPushLine;
    screenCallbacks.sb_popline = onSbPopLine;
    screenCallbacks.sb_clear = onSbClear;
    vterm_screen_set_callbacks(m_vtermScreen, &screenCallbacks, this);
    clearScrollback();
    vterm_screen_reset(m_vtermScreen, 1);

    if (!m_pty->start(workingDirectory)) {
        vterm_free(m_vterm);
        m_vterm = nullptr;
        m_vtermScreen = nullptr;
        return false;
    }

    m_pty->resize(m_cols, m_rows);
    update();
    return true;
}

void VtermTerminalWidget::stopShell()
{
    clearScrollback();

    if (m_pty) {
        m_pty->stop();
    }

    if (m_vterm) {
        vterm_free(m_vterm);
        m_vterm = nullptr;
        m_vtermScreen = nullptr;
    }
}

bool VtermTerminalWidget::isRunning() const
{
    return m_pty && m_pty->isRunning();
}

QSize VtermTerminalWidget::sizeHint() const
{
    return {m_cellWidth * m_cols + 8, m_cellHeight * m_rows + 8};
}

void VtermTerminalWidget::updateTerminalSize()
{
    const int cols = qMax(10, (width() - 4) / m_cellWidth);
    const int rows = qMax(4, (height() - 4) / m_cellHeight);
    m_cols = cols;
    m_rows = rows;

    if (m_vterm) {
        vterm_set_size(m_vterm, rows, cols);
    }
    if (m_vtermScreen) {
        vterm_screen_flush_damage(m_vtermScreen);
    }
    if (m_pty && m_pty->isRunning()) {
        m_pty->resize(cols, rows);
    }
}

void VtermTerminalWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTerminalSize();
}

void VtermTerminalWidget::onPtyReadyRead(const QByteArray &data)
{
    if (!m_vterm || data.isEmpty()) {
        return;
    }

    vterm_input_write(m_vterm, data.constData(), static_cast<size_t>(data.size()));
    if (m_vtermScreen) {
        vterm_screen_flush_damage(m_vtermScreen);
    }
    clampScrollOffset();
}

void VtermTerminalWidget::onPtyExited(int exitCode)
{
    stopShell();
    emit shellExited(exitCode);
    update();
}

void VtermTerminalWidget::setGridSize(int rows, int columns)
{
    m_rows = rows;
    m_cols = columns;
    update();
}

void VtermTerminalWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));

    if (!m_vtermScreen) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, tr("Terminal not started"));
        return;
    }

    const QColor defaultFg = QColor(0xd4, 0xd4, 0xd4);
    const QColor defaultBg = QColor(0x1e, 0x1e, 0x1e);

    painter.setFont(font());

    for (int row = 0; row < m_rows; ++row) {
        const int displayLine = static_cast<int>(m_scrollback.size()) - m_scrollOffset + row;
        for (int col = 0; col < m_cols; ++col) {
            VTermScreenCell cell{};
            if (!cellAtDisplayLine(displayLine, col, &cell)) {
                continue;
            }

            const QColor fg = colorFromVterm(&cell.fg, true, defaultFg);
            const QColor bg = colorFromVterm(&cell.bg, false, defaultBg);

            const QRect cellRect(col * m_cellWidth + 2, row * m_cellHeight + 2, m_cellWidth,
                                 m_cellHeight);
            painter.fillRect(cellRect, bg);

            QString text;
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i] != 0; ++i) {
                text.append(QChar::fromUcs4(cell.chars[i]));
            }
            if (text.isEmpty()) {
                continue;
            }

            painter.setPen(fg);
            painter.drawText(cellRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    }
}

void VtermTerminalWidget::sendKey(VTermKey key, VTermModifier mod)
{
    if (!m_vterm || !m_pty || key == VTERM_KEY_NONE) {
        return;
    }

    vterm_keyboard_key(m_vterm, key, mod);
}

void VtermTerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_vterm || !m_pty) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (m_scrollOffset > 0) {
        m_scrollOffset = 0;
        update();
    }

    const VTermModifier mod = qtModifiersToVterm(event->modifiers());
    const VTermKey key = qtKeyToVterm(event->key());

    if (key != VTERM_KEY_NONE) {
        sendKey(key, mod);
        event->accept();
        return;
    }

    const QString text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint()) {
        const uint32_t codepoint = text.at(0).unicode();
        vterm_keyboard_unichar(m_vterm, codepoint, mod);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void VtermTerminalWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    QWidget::mousePressEvent(event);
}

void VtermTerminalWidget::focusInEvent(QFocusEvent *event)
{
    m_hasFocus = true;
    QWidget::focusInEvent(event);
}

void VtermTerminalWidget::focusOutEvent(QFocusEvent *event)
{
    m_hasFocus = false;
    QWidget::focusOutEvent(event);
}

void VtermTerminalWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_vtermScreen) {
        QWidget::wheelEvent(event);
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const int lines = qMax(1, qAbs(delta) / 120);
    if (delta > 0) {
        m_scrollOffset = qMin(m_scrollOffset + lines, static_cast<int>(m_scrollback.size()));
    } else {
        m_scrollOffset = qMax(0, m_scrollOffset - lines);
    }
    update();
    event->accept();
}

void VtermTerminalWidget::clearScrollback()
{
    m_scrollback.clear();
    m_scrollOffset = 0;
}

bool VtermTerminalWidget::scrollbackPush(int cols, const VTermScreenCell *cells)
{
    if (!cells || cols < 1) {
        return false;
    }

    QVector<VTermScreenCell> line;
    line.reserve(cols);
    for (int col = 0; col < cols; ++col) {
        line.append(cells[col]);
    }
    m_scrollback.append(line);

    while (m_scrollback.size() > kMaxScrollbackLines) {
        m_scrollback.removeFirst();
        if (m_scrollOffset > 0) {
            --m_scrollOffset;
        }
    }

    clampScrollOffset();
    return true;
}

bool VtermTerminalWidget::scrollbackPop(int cols, VTermScreenCell *cells)
{
    if (!cells || cols < 1 || m_scrollback.isEmpty()) {
        return false;
    }

    const QVector<VTermScreenCell> line = m_scrollback.takeLast();
    if (m_scrollOffset > 0) {
        --m_scrollOffset;
    }

    for (int col = 0; col < cols; ++col) {
        if (col < line.size()) {
            cells[col] = line[col];
        } else {
            cells[col] = {};
        }
    }
    return true;
}

bool VtermTerminalWidget::cellAtDisplayLine(int displayLine, int col,
                                            VTermScreenCell *cell) const
{
    if (!cell || col < 0) {
        return false;
    }

    if (displayLine >= 0 && displayLine < m_scrollback.size()) {
        const QVector<VTermScreenCell> &line = m_scrollback.at(displayLine);
        if (col < line.size()) {
            *cell = line.at(col);
            return true;
        }
        *cell = {};
        return true;
    }

    if (!m_vtermScreen) {
        return false;
    }

    const int screenRow = displayLine - static_cast<int>(m_scrollback.size());
    if (screenRow < 0 || screenRow >= m_rows || col >= m_cols) {
        return false;
    }

    VTermPos pos{};
    pos.row = screenRow;
    pos.col = col;
    return vterm_screen_get_cell(m_vtermScreen, pos, cell) != 0;
}

void VtermTerminalWidget::clampScrollOffset()
{
    m_scrollOffset = qBound(0, m_scrollOffset, static_cast<int>(m_scrollback.size()));
}

int VtermTerminalWidget::onSbPushLine(int cols, const VTermScreenCell *cells, void *user)
{
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    return widget->scrollbackPush(cols, cells) ? 1 : 0;
}

int VtermTerminalWidget::onSbPopLine(int cols, VTermScreenCell *cells, void *user)
{
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    return widget->scrollbackPop(cols, cells) ? 1 : 0;
}

int VtermTerminalWidget::onSbClear(void *user)
{
    auto *widget = static_cast<VtermTerminalWidget *>(user);
    widget->clearScrollback();
    return 1;
}
