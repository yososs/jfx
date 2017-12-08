/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Text.h"

#include "Event.h"
#include "RenderCombineText.h"
#include "RenderSVGInlineText.h"
#include "RenderText.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "StyleUpdate.h"
#include "TextNodeTraversal.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<Text> Text::create(Document& document, const String& data)
{
    return adoptRef(*new Text(document, data, CreateText));
}

Ref<Text> Text::createEditingText(Document& document, const String& data)
{
    return adoptRef(*new Text(document, data, CreateEditingText));
}

Text::~Text()
{
}

ExceptionOr<Ref<Text>> Text::splitText(unsigned offset)
{
    if (offset > length())
        return Exception { IndexSizeError };

    EventQueueScope scope;
    auto oldData = data();
    auto newText = virtualCreate(oldData.substring(offset));
    setDataWithoutUpdate(oldData.substring(0, offset));

    dispatchModifiedEvent(oldData);

    if (auto* parent = parentNode()) {
        auto insertResult = parent->insertBefore(newText, nextSibling());
        if (insertResult.hasException())
            return insertResult.releaseException();
    }

    document().textNodeSplit(this);

    if (renderer())
        renderer()->setTextWithOffset(data(), 0, oldData.length());

    return WTFMove(newText);
}

static const Text* earliestLogicallyAdjacentTextNode(const Text* text)
{
    const Node* node = text;
    while ((node = node->previousSibling())) {
        if (!is<Text>(*node))
            break;
        text = downcast<Text>(node);
    }
    return text;
}

static const Text* latestLogicallyAdjacentTextNode(const Text* text)
{
    const Node* node = text;
    while ((node = node->nextSibling())) {
        if (!is<Text>(*node))
            break;
        text = downcast<Text>(node);
    }
    return text;
}

String Text::wholeText() const
{
    const Text* startText = earliestLogicallyAdjacentTextNode(this);
    const Text* endText = latestLogicallyAdjacentTextNode(this);
    ASSERT(endText);
    const Node* onePastEndText = TextNodeTraversal::nextSibling(*endText);

    StringBuilder result;
    for (const Text* text = startText; text != onePastEndText; text = TextNodeTraversal::nextSibling(*text))
        result.append(text->data());
    return result.toString();
}

RefPtr<Text> Text::replaceWholeText(const String& newText)
{
    // Remove all adjacent text nodes, and replace the contents of this one.

    // Protect startText and endText against mutation event handlers removing the last ref
    RefPtr<Text> startText = const_cast<Text*>(earliestLogicallyAdjacentTextNode(this));
    RefPtr<Text> endText = const_cast<Text*>(latestLogicallyAdjacentTextNode(this));

    RefPtr<Text> protectedThis(this); // Mutation event handlers could cause our last ref to go away
    RefPtr<ContainerNode> parent = parentNode(); // Protect against mutation handlers moving this node during traversal
    for (RefPtr<Node> n = startText; n && n != this && n->isTextNode() && n->parentNode() == parent;) {
        Ref<Node> nodeToRemove(n.releaseNonNull());
        n = nodeToRemove->nextSibling();
        parent->removeChild(nodeToRemove);
    }

    if (this != endText) {
        Node* onePastEndText = endText->nextSibling();
        for (RefPtr<Node> n = nextSibling(); n && n != onePastEndText && n->isTextNode() && n->parentNode() == parent;) {
            Ref<Node> nodeToRemove(n.releaseNonNull());
            n = nodeToRemove->nextSibling();
            parent->removeChild(nodeToRemove);
        }
    }

    if (newText.isEmpty()) {
        if (parent && parentNode() == parent)
            parent->removeChild(*this);
        return nullptr;
    }

    setData(newText);
    return protectedThis;
}

String Text::nodeName() const
{
    return ASCIILiteral("#text");
}

Node::NodeType Text::nodeType() const
{
    return TEXT_NODE;
}

Ref<Node> Text::cloneNodeInternal(Document& targetDocument, CloningOperation)
{
    return create(targetDocument, data());
}

static bool isSVGShadowText(Text* text)
{
    Node* parentNode = text->parentNode();
    ASSERT(parentNode);
    return is<ShadowRoot>(*parentNode) && downcast<ShadowRoot>(*parentNode).host()->hasTagName(SVGNames::trefTag);
}

static bool isSVGText(Text* text)
{
    Node* parentOrShadowHostNode = text->parentOrShadowHostNode();
    return parentOrShadowHostNode->isSVGElement() && !parentOrShadowHostNode->hasTagName(SVGNames::foreignObjectTag);
}

RenderPtr<RenderText> Text::createTextRenderer(const RenderStyle& style)
{
    if (isSVGText(this) || isSVGShadowText(this))
        return createRenderer<RenderSVGInlineText>(*this, data());

    if (style.hasTextCombine())
        return createRenderer<RenderCombineText>(*this, data());

    return createRenderer<RenderText>(*this, data());
}

bool Text::childTypeAllowed(NodeType) const
{
    return false;
}

Ref<Text> Text::virtualCreate(const String& data)
{
    return create(document(), data);
}

Ref<Text> Text::createWithLengthLimit(Document& document, const String& data, unsigned start, unsigned lengthLimit)
{
    unsigned dataLength = data.length();

    if (!start && dataLength <= lengthLimit)
        return create(document, data);

    Ref<Text> result = Text::create(document, String());
    result->parserAppendData(data, start, lengthLimit);
    return result;
}

void Text::updateRendererAfterContentChange(unsigned offsetOfReplacedData, unsigned lengthOfReplacedData)
{
    ASSERT(parentNode());
    if (styleValidity() >= Style::Validity::SubtreeAndRenderersInvalid)
        return;

    document().updateTextRenderer(*this, offsetOfReplacedData, lengthOfReplacedData);
}

#if ENABLE(TREE_DEBUGGING)
void Text::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    result.append(nodeName());

    s = data();
    if (s.length() > 0) {
        if (result.length())
            result.appendLiteral("; ");
        result.appendLiteral("length=");
        result.appendNumber(s.length());
        result.appendLiteral("; value=\"");
        result.append(s);
        result.append('"');
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
    buffer[length - 1] = '\0';
}
#endif

} // namespace WebCore
