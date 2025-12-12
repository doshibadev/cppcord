#include "DiscordMarkdown.h"
#include <QRegularExpression>

QString DiscordMarkdown::escapeHtml(const QString &text)
{
    QString result = text;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    result.replace("'", "&#39;");
    return result;
}

QString DiscordMarkdown::parseCodeBlocks(const QString &text)
{
    QString result = text;

    // Match ```language\ncode``` blocks
    QRegularExpression codeBlockRegex(R"(```(?:(\w+)\n)?([\s\S]*?)```)");
    QRegularExpressionMatchIterator it = codeBlockRegex.globalMatch(result);

    QList<QPair<int, QPair<int, QString>>> replacements;
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        QString language = match.captured(1);
        QString code = match.captured(2);

        QString html = QString("<pre style='background: #2f3136; padding: 8px; border-radius: 4px; "
                               "overflow-x: auto; margin: 4px 0;'><code style='color: #dcddde; "
                               "font-family: Consolas, monospace; font-size: 14px;'>%1</code></pre>")
                           .arg(escapeHtml(code));

        replacements.append(qMakePair(match.capturedStart(), qMakePair(match.capturedLength(), html)));
    }

    // Apply replacements in reverse order to maintain positions
    for (int i = replacements.size() - 1; i >= 0; --i)
    {
        result.replace(replacements[i].first, replacements[i].second.first, replacements[i].second.second);
    }

    return result;
}

QString DiscordMarkdown::parseInlineFormatting(const QString &text)
{
    QString result = text;

    // Inline code `code` - do this first to avoid formatting inside code
    QRegularExpression inlineCodeRegex(R"(`([^`]+)`)");
    result.replace(inlineCodeRegex,
                   "<code style='background: #2f3136; padding: 2px 4px; border-radius: 3px; "
                   "font-family: Consolas, monospace; font-size: 13px;'>\\1</code>");

    // Bold/Italic combinations ***text*** or ___text___
    QRegularExpression boldItalicRegex(R"((\*\*\*|___)(.+?)\1)");
    result.replace(boldItalicRegex, "<strong><em>\\2</em></strong>");

    // Bold **text** or __text__
    QRegularExpression boldRegex(R"((\*\*|__)(.+?)\1)");
    result.replace(boldRegex, "<strong>\\2</strong>");

    // Italic *text* or _text_ (single asterisk/underscore)
    QRegularExpression italicRegex(R"((?<!\*)\*(?!\*)([^\*]+)\*(?!\*)|(?<!_)_(?!_)([^_]+)_(?!_))");
    result.replace(italicRegex, "<em>\\1\\2</em>");

    // Strikethrough ~~text~~
    QRegularExpression strikeRegex(R"(~~(.+?)~~)");
    result.replace(strikeRegex, "<s>\\1</s>");

    // Underline (Discord uses __text__ for both bold and underline, we already did bold)
    // Discord's actual underline is __text__ but we handle it as bold above

    return result;
}

QString DiscordMarkdown::parseBlockquotes(const QString &text)
{
    QString result = text;

    // Multi-line blockquote >>>
    if (result.contains(">>>"))
    {
        QRegularExpression multiQuoteRegex(R"(^>>> (.+)$)", QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption);
        result.replace(multiQuoteRegex,
                       "<blockquote style='border-left: 4px solid #4f545c; padding-left: 12px; "
                       "margin: 4px 0; color: #dcddde;'>\\1</blockquote>");
    }

    // Single line blockquote >
    QRegularExpression singleQuoteRegex(R"(^> (.+)$)", QRegularExpression::MultilineOption);
    result.replace(singleQuoteRegex,
                   "<blockquote style='border-left: 4px solid #4f545c; padding-left: 12px; "
                   "margin: 4px 0; color: #dcddde;'>\\1</blockquote>");

    return result;
}

QString DiscordMarkdown::parseHeadings(const QString &text)
{
    QString result = text;

    // H3 ###
    QRegularExpression h3Regex(R"(^### (.+)$)", QRegularExpression::MultilineOption);
    result.replace(h3Regex, "<h3 style='font-size: 16px; font-weight: 600; margin: 8px 0;'>\\1</h3>");

    // H2 ##
    QRegularExpression h2Regex(R"(^## (.+)$)", QRegularExpression::MultilineOption);
    result.replace(h2Regex, "<h2 style='font-size: 18px; font-weight: 600; margin: 8px 0;'>\\1</h2>");

    // H1 #
    QRegularExpression h1Regex(R"(^# (.+)$)", QRegularExpression::MultilineOption);
    result.replace(h1Regex, "<h1 style='font-size: 20px; font-weight: 600; margin: 8px 0;'>\\1</h1>");

    return result;
}

QString DiscordMarkdown::parseLists(const QString &text)
{
    QString result = text;

    // Unordered lists - or *
    QRegularExpression ulRegex(R"(^[*-] (.+)$)", QRegularExpression::MultilineOption);
    result.replace(ulRegex, "<li style='margin-left: 20px;'>\\1</li>");

    // Ordered lists 1. 2. etc
    QRegularExpression olRegex(R"(^\d+\. (.+)$)", QRegularExpression::MultilineOption);
    result.replace(olRegex, "<li style='margin-left: 20px; list-style-type: decimal;'>\\1</li>");

    return result;
}

QString DiscordMarkdown::parseLinks(const QString &text)
{
    QString result = text;

    // Markdown links [text](url)
    QRegularExpression linkRegex(R"(\[([^\]]+)\]\(([^\)]+)\))");
    result.replace(linkRegex,
                   "<a href='\\2' style='color: #00b0f4; text-decoration: none;'>\\1</a>");

    return result;
}

QString DiscordMarkdown::autoLinkUrls(const QString &text)
{
    QString result = text;

    // Auto-link URLs (http, https)
    // Don't auto-link if already in <a> tag or markdown link
    QRegularExpression urlRegex(R"(\b(https?://[^\s<]+))");
    result.replace(urlRegex,
                   "<a href='\\1' style='color: #00b0f4; text-decoration: none;'>\\1</a>");

    return result;
}

QString DiscordMarkdown::toHtml(const QString &markdown)
{
    if (markdown.isEmpty())
        return QString();

    // Order matters! Process in the correct sequence:

    // 1. Escape HTML first (but we'll handle this differently for code blocks)
    QString result = markdown;

    // 2. Parse code blocks first (they should not be processed for other markdown)
    result = parseCodeBlocks(result);

    // 3. Parse blockquotes
    result = parseBlockquotes(result);

    // 4. Parse headings
    result = parseHeadings(result);

    // 5. Parse lists
    result = parseLists(result);

    // 6. Parse links (markdown format)
    result = parseLinks(result);

    // 7. Auto-link URLs
    result = autoLinkUrls(result);

    // 8. Parse inline formatting (bold, italic, strikethrough, inline code)
    result = parseInlineFormatting(result);

    // 9. Convert newlines to <br>
    result.replace("\n", "<br>");

    return result;
}
