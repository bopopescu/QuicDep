/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGEllipseElement.h"

#include "core/css/StyleChangeReason.h"
#include "core/layout/svg/LayoutSVGEllipse.h"
#include "core/svg/SVGLength.h"

namespace blink {

inline SVGEllipseElement::SVGEllipseElement(Document& document)
    : SVGGeometryElement(SVGNames::ellipseTag, document),
      cx_(SVGAnimatedLength::Create(this,
                                    SVGNames::cxAttr,
                                    SVGLength::Create(SVGLengthMode::kWidth),
                                    CSSPropertyCx)),
      cy_(SVGAnimatedLength::Create(this,
                                    SVGNames::cyAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight),
                                    CSSPropertyCy)),
      rx_(SVGAnimatedLength::Create(this,
                                    SVGNames::rxAttr,
                                    SVGLength::Create(SVGLengthMode::kWidth),
                                    CSSPropertyRx)),
      ry_(SVGAnimatedLength::Create(this,
                                    SVGNames::ryAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight),
                                    CSSPropertyRy)) {
  AddToPropertyMap(cx_);
  AddToPropertyMap(cy_);
  AddToPropertyMap(rx_);
  AddToPropertyMap(ry_);
}

void SVGEllipseElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(cx_);
  visitor->Trace(cy_);
  visitor->Trace(rx_);
  visitor->Trace(ry_);
  SVGGeometryElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGEllipseElement)

Path SVGEllipseElement::AsPath() const {
  Path path;

  SVGLengthContext length_context(this);
  DCHECK(GetLayoutObject());
  const ComputedStyle& style = GetLayoutObject()->StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();

  FloatSize radii(ToFloatSize(
      length_context.ResolveLengthPair(svg_style.Rx(), svg_style.Ry(), style)));
  if (radii.Width() < 0 || radii.Height() < 0 ||
      (!radii.Width() && !radii.Height()))
    return path;

  FloatPoint center(
      length_context.ResolveLengthPair(svg_style.Cx(), svg_style.Cy(), style));
  path.AddEllipse(FloatRect(center - radii, radii.ScaledBy(2)));
  return path;
}

void SVGEllipseElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == cx_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            cx_->CssValue());
  } else if (property == cy_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            cy_->CssValue());
  } else if (property == rx_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            rx_->CssValue());
  } else if (property == ry_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            ry_->CssValue());
  } else {
    SVGGeometryElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGEllipseElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::cxAttr || attr_name == SVGNames::cyAttr ||
      attr_name == SVGNames::rxAttr || attr_name == SVGNames::ryAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
    UpdateRelativeLengthsInformation();

    LayoutSVGShape* layout_object = ToLayoutSVGShape(this->GetLayoutObject());
    if (!layout_object)
      return;

    layout_object->SetNeedsShapeUpdate();
    MarkForLayoutAndParentResourceInvalidation(layout_object);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(attr_name);
}

bool SVGEllipseElement::SelfHasRelativeLengths() const {
  return cx_->CurrentValue()->IsRelative() ||
         cy_->CurrentValue()->IsRelative() ||
         rx_->CurrentValue()->IsRelative() || ry_->CurrentValue()->IsRelative();
}

LayoutObject* SVGEllipseElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGEllipse(this);
}

}  // namespace blink
