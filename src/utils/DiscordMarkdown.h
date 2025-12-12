#pragma once
#include <QString>
#include <QRegularExpression>

/**
 * @brief Discord markdown parser - converts Discord-flavored markdown to HTML
 *
 * Supports:
 * - Bold (**text** or __text__)
 * - Italic (*text* or _text_)
 * - Strikethrough (~~text~~)
 * - Underline (__text__)
 * - Code (`code`)
 * - Code blocks (```language\ncode```)
 * - Blockquotes (> text or >>> multiline)
 * - Links (markdown and auto-linking)
 * - Headings (# H1 through ### H3)
 * - Lists (ordered and unordered)
 */
class DiscordMarkdown
{
public:
    /**
     * @brief Parse Discord markdown to HTML
     * @param markdown The Discord markdown text
     * @return HTML formatted string ready for display
     */
    static QString toHtml(const QString &markdown);

private:
    static QString escapeHtml(const QString &text);
    static QString parseInlineFormatting(const QString &text);
    static QString parseCodeBlocks(const QString &text);
    static QString parseBlockquotes(const QString &text);
    static QString parseHeadings(const QString &text);
    static QString parseLists(const QString &text);
    static QString parseLinks(const QString &text);
    static QString autoLinkUrls(const QString &text);
};
