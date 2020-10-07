// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_object.h"

#include <set>
#include <tuple>

#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/xml/cfx_xmlelement.h"
#include "core/fxcrt/xml/cfx_xmltext.h"
#include "fxjs/cjs_result.h"
#include "fxjs/gc/container_trace.h"
#include "fxjs/xfa/cfxjse_engine.h"
#include "fxjs/xfa/cfxjse_value.h"
#include "fxjs/xfa/cjx_boolean.h"
#include "fxjs/xfa/cjx_draw.h"
#include "fxjs/xfa/cjx_field.h"
#include "fxjs/xfa/cjx_instancemanager.h"
#include "third_party/base/compiler_specific.h"
#include "third_party/base/stl_util.h"
#include "xfa/fgas/crt/cfgas_decimal.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/parser/cxfa_border.h"
#include "xfa/fxfa/parser/cxfa_datavalue.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_edge.h"
#include "xfa/fxfa/parser/cxfa_fill.h"
#include "xfa/fxfa/parser/cxfa_font.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_object.h"
#include "xfa/fxfa/parser/cxfa_occur.h"
#include "xfa/fxfa/parser/cxfa_proto.h"
#include "xfa/fxfa/parser/cxfa_subform.h"
#include "xfa/fxfa/parser/cxfa_validate.h"
#include "xfa/fxfa/parser/cxfa_value.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"
#include "xfa/fxfa/parser/xfa_utils.h"

namespace {

void XFA_DeleteWideString(void* pData) {
  delete static_cast<WideString*>(pData);
}

void XFA_CopyWideString(void*& pData) {
  if (!pData)
    return;
  pData = new WideString(*reinterpret_cast<WideString*>(pData));
}

enum XFA_KEYTYPE {
  XFA_KEYTYPE_Custom,
  XFA_KEYTYPE_Element,
};

uint32_t GetMapKey_Custom(WideStringView wsKey) {
  uint32_t dwKey = FX_HashCode_GetW(wsKey, false);
  return ((dwKey << 1) | XFA_KEYTYPE_Custom);
}

uint32_t GetMapKey_Element(XFA_Element eType, XFA_Attribute eAttribute) {
  return ((static_cast<uint32_t>(eType) << 16) |
          (static_cast<uint32_t>(eAttribute) << 8) | XFA_KEYTYPE_Element);
}

std::tuple<int32_t, int32_t, int32_t> StrToRGB(const WideString& strRGB) {
  int32_t r = 0;
  int32_t g = 0;
  int32_t b = 0;

  size_t iIndex = 0;
  for (size_t i = 0; i < strRGB.GetLength(); ++i) {
    wchar_t ch = strRGB[i];
    if (ch == L',')
      ++iIndex;
    if (iIndex > 2)
      break;

    int32_t iValue = ch - L'0';
    if (iValue >= 0 && iValue <= 9) {
      switch (iIndex) {
        case 0:
          r = r * 10 + iValue;
          break;
        case 1:
          g = g * 10 + iValue;
          break;
        default:
          b = b * 10 + iValue;
          break;
      }
    }
  }
  return {r, g, b};
}

}  // namespace

struct XFA_MAPDATABLOCKCALLBACKINFO {
  void (*pFree)(void* pData);
  void (*pCopy)(void*& pData);
};

struct XFA_MAPDATABLOCK {
  static size_t SizeForCapacity(size_t capacity) {
    return sizeof(XFA_MAPDATABLOCK) + capacity;
  }
  uint8_t* GetData() const { return (uint8_t*)this + sizeof(XFA_MAPDATABLOCK); }

  const XFA_MAPDATABLOCKCALLBACKINFO* pCallbackInfo;
  size_t iBytes;
};

struct XFA_MAPMODULEDATA {
  XFA_MAPMODULEDATA() {}
  ~XFA_MAPMODULEDATA() {}

  // These two are keyed by result of GetMapKey_*().
  std::map<uint32_t, int32_t> m_ValueMap;  // int/enum/bool represented as int.
  std::map<uint32_t, XFA_MAPDATABLOCK*> m_BufferMap;
};

const XFA_MAPDATABLOCKCALLBACKINFO deleteWideStringCallBack = {
    XFA_DeleteWideString, XFA_CopyWideString};

CJX_Object::CJX_Object(CXFA_Object* obj) : object_(obj) {}

CJX_Object::~CJX_Object() {
  ClearMapModuleBuffer();
}

CJX_Object* CJX_Object::AsCJXObject() {
  return this;
}

void CJX_Object::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(object_);
  visitor->Trace(layout_item_);
  visitor->Trace(calc_data_);
}

bool CJX_Object::DynamicTypeIs(TypeTag eType) const {
  return eType == static_type__;
}

void CJX_Object::DefineMethods(pdfium::span<const CJX_MethodSpec> methods) {
  for (const auto& item : methods)
    method_specs_[item.pName] = item.pMethodCall;
}

CXFA_Document* CJX_Object::GetDocument() const {
  return object_->GetDocument();
}

CXFA_Node* CJX_Object::GetXFANode() const {
  return ToNode(GetXFAObject());
}

void CJX_Object::className(CFXJSE_Value* pValue,
                           bool bSetting,
                           XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException();
    return;
  }
  pValue->SetString(GetXFAObject()->GetClassName());
}

int32_t CJX_Object::Subform_and_SubformSet_InstanceIndex() {
  int32_t index = 0;
  for (CXFA_Node* pNode = GetXFANode()->GetPrevSibling(); pNode;
       pNode = pNode->GetPrevSibling()) {
    if ((pNode->GetElementType() != XFA_Element::Subform) &&
        (pNode->GetElementType() != XFA_Element::SubformSet)) {
      break;
    }
    index++;
  }
  return index;
}

bool CJX_Object::HasMethod(const WideString& func) const {
  return pdfium::Contains(method_specs_, func.ToUTF8());
}

CJS_Result CJX_Object::RunMethod(
    const WideString& func,
    const std::vector<v8::Local<v8::Value>>& params) {
  auto it = method_specs_.find(func.ToUTF8());
  if (it == method_specs_.end())
    return CJS_Result::Failure(JSMessage::kUnknownMethod);

  return it->second(this, GetXFAObject()->GetDocument()->GetScriptContext(),
                    params);
}

void CJX_Object::ThrowTooManyOccurancesException(const WideString& obj) const {
  ThrowException(WideString::FromASCII("The element [") + obj +
                 WideString::FromASCII(
                     "] has violated its allowable number of occurrences."));
}

void CJX_Object::ThrowInvalidPropertyException() const {
  ThrowException(WideString::FromASCII("Invalid property set operation."));
}

void CJX_Object::ThrowIndexOutOfBoundsException() const {
  ThrowException(WideString::FromASCII("Index value is out of bounds."));
}

void CJX_Object::ThrowParamCountMismatchException(
    const WideString& method) const {
  ThrowException(
      WideString::FromASCII("Incorrect number of parameters calling method '") +
      method + WideString::FromASCII("'."));
}

void CJX_Object::ThrowArgumentMismatchException() const {
  ThrowException(WideString::FromASCII(
      "Argument mismatch in property or function argument."));
}

void CJX_Object::ThrowException(const WideString& str) const {
  ASSERT(!str.IsEmpty());
  FXJSE_ThrowMessage(str.ToUTF8().AsStringView());
}

bool CJX_Object::HasAttribute(XFA_Attribute eAttr) {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  return HasMapModuleKey(key);
}

void CJX_Object::SetAttributeByEnum(XFA_Attribute eAttr,
                                    WideStringView wsValue,
                                    bool bNotify) {
  switch (GetXFANode()->GetAttributeType(eAttr)) {
    case XFA_AttributeType::Enum: {
      Optional<XFA_AttributeValue> item = XFA_GetAttributeValueByName(wsValue);
      SetEnum(eAttr, item ? *item : *(GetXFANode()->GetDefaultEnum(eAttr)),
              bNotify);
      break;
    }
    case XFA_AttributeType::CData:
      SetCDataImpl(eAttr, WideString(wsValue), bNotify, false);
      break;
    case XFA_AttributeType::Boolean:
      SetBoolean(eAttr, !wsValue.EqualsASCII("0"), bNotify);
      break;
    case XFA_AttributeType::Integer:
      SetInteger(eAttr,
                 FXSYS_roundf(FXSYS_wcstof(wsValue.unterminated_c_str(),
                                           wsValue.GetLength(), nullptr)),
                 bNotify);
      break;
    case XFA_AttributeType::Measure:
      SetMeasure(eAttr, CXFA_Measurement(wsValue), bNotify);
      break;
    default:
      break;
  }
}

void CJX_Object::SetMapModuleString(uint32_t key, WideStringView wsValue) {
  SetMapModuleBuffer(key, const_cast<wchar_t*>(wsValue.unterminated_c_str()),
                     wsValue.GetLength() * sizeof(wchar_t), nullptr);
}

void CJX_Object::SetAttributeByString(WideStringView wsAttr,
                                      WideStringView wsValue) {
  Optional<XFA_ATTRIBUTEINFO> attr = XFA_GetAttributeByName(wsAttr);
  if (attr.has_value()) {
    SetAttributeByEnum(attr.value().attribute, wsValue, true);
    return;
  }
  uint32_t key = GetMapKey_Custom(wsAttr);
  SetMapModuleString(key, wsValue);
}

WideString CJX_Object::GetAttributeByString(WideStringView attr) {
  Optional<WideString> result;
  Optional<XFA_ATTRIBUTEINFO> enum_attr = XFA_GetAttributeByName(attr);
  if (enum_attr.has_value())
    result = TryAttribute(enum_attr.value().attribute, true);
  else
    result = GetMapModuleString(GetMapKey_Custom(attr));
  return result.value_or(WideString());
}

WideString CJX_Object::GetAttributeByEnum(XFA_Attribute attr) {
  return TryAttribute(attr, true).value_or(WideString());
}

Optional<WideString> CJX_Object::TryAttribute(XFA_Attribute eAttr,
                                              bool bUseDefault) {
  switch (GetXFANode()->GetAttributeType(eAttr)) {
    case XFA_AttributeType::Enum: {
      Optional<XFA_AttributeValue> value = TryEnum(eAttr, bUseDefault);
      if (!value)
        return {};
      return WideString::FromASCII(XFA_AttributeValueToName(*value));
    }
    case XFA_AttributeType::CData:
      return TryCData(eAttr, bUseDefault);

    case XFA_AttributeType::Boolean: {
      Optional<bool> value = TryBoolean(eAttr, bUseDefault);
      if (!value)
        return {};
      return WideString(*value ? L"1" : L"0");
    }
    case XFA_AttributeType::Integer: {
      Optional<int32_t> iValue = TryInteger(eAttr, bUseDefault);
      if (!iValue)
        return {};
      return WideString::Format(L"%d", *iValue);
    }
    case XFA_AttributeType::Measure: {
      Optional<CXFA_Measurement> value = TryMeasure(eAttr, bUseDefault);
      if (!value)
        return {};

      return value->ToString();
    }
    default:
      break;
  }
  return {};
}

void CJX_Object::RemoveAttribute(WideStringView wsAttr) {
  RemoveMapModuleKey(GetMapKey_Custom(wsAttr));
}

Optional<bool> CJX_Object::TryBoolean(XFA_Attribute eAttr, bool bUseDefault) {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  Optional<int32_t> value = GetMapModuleValue(key);
  if (value.has_value())
    return !!value.value();
  if (!bUseDefault)
    return pdfium::nullopt;
  return GetXFANode()->GetDefaultBoolean(eAttr);
}

void CJX_Object::SetBoolean(XFA_Attribute eAttr, bool bValue, bool bNotify) {
  CFX_XMLElement* elem = SetValue(eAttr, static_cast<int32_t>(bValue), bNotify);
  if (elem) {
    elem->SetAttribute(WideString::FromASCII(XFA_AttributeToName(eAttr)),
                       bValue ? L"1" : L"0");
  }
}

bool CJX_Object::GetBoolean(XFA_Attribute eAttr) {
  return TryBoolean(eAttr, true).value_or(false);
}

void CJX_Object::SetInteger(XFA_Attribute eAttr, int32_t iValue, bool bNotify) {
  CFX_XMLElement* elem = SetValue(eAttr, iValue, bNotify);
  if (elem) {
    elem->SetAttribute(WideString::FromASCII(XFA_AttributeToName(eAttr)),
                       WideString::Format(L"%d", iValue));
  }
}

int32_t CJX_Object::GetInteger(XFA_Attribute eAttr) const {
  return TryInteger(eAttr, true).value_or(0);
}

Optional<int32_t> CJX_Object::TryInteger(XFA_Attribute eAttr,
                                         bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  Optional<int32_t> value = GetMapModuleValue(key);
  if (value.has_value())
    return value.value();
  if (!bUseDefault)
    return pdfium::nullopt;
  return GetXFANode()->GetDefaultInteger(eAttr);
}

Optional<XFA_AttributeValue> CJX_Object::TryEnum(XFA_Attribute eAttr,
                                                 bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  Optional<int32_t> value = GetMapModuleValue(key);
  if (value.has_value())
    return static_cast<XFA_AttributeValue>(value.value());
  if (!bUseDefault)
    return pdfium::nullopt;
  return GetXFANode()->GetDefaultEnum(eAttr);
}

void CJX_Object::SetEnum(XFA_Attribute eAttr,
                         XFA_AttributeValue eValue,
                         bool bNotify) {
  CFX_XMLElement* elem = SetValue(eAttr, static_cast<int32_t>(eValue), bNotify);
  if (elem) {
    elem->SetAttribute(WideString::FromASCII(XFA_AttributeToName(eAttr)),
                       WideString::FromASCII(XFA_AttributeValueToName(eValue)));
  }
}

XFA_AttributeValue CJX_Object::GetEnum(XFA_Attribute eAttr) const {
  return TryEnum(eAttr, true).value_or(XFA_AttributeValue::Unknown);
}

void CJX_Object::SetMeasure(XFA_Attribute eAttr,
                            CXFA_Measurement mValue,
                            bool bNotify) {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  OnChanging(eAttr, bNotify);
  SetMapModuleBuffer(key, &mValue, sizeof(CXFA_Measurement), nullptr);
  OnChanged(eAttr, bNotify, false);
}

Optional<CXFA_Measurement> CJX_Object::TryMeasure(XFA_Attribute eAttr,
                                                  bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  void* pValue;
  int32_t iBytes;
  if (GetMapModuleBuffer(key, &pValue, &iBytes) &&
      iBytes == sizeof(CXFA_Measurement)) {
    return *static_cast<CXFA_Measurement*>(pValue);
  }
  if (!bUseDefault)
    return {};
  return GetXFANode()->GetDefaultMeasurement(eAttr);
}

Optional<float> CJX_Object::TryMeasureAsFloat(XFA_Attribute attr) const {
  Optional<CXFA_Measurement> measure = TryMeasure(attr, false);
  if (measure)
    return measure->ToUnit(XFA_Unit::Pt);
  return pdfium::nullopt;
}

CXFA_Measurement CJX_Object::GetMeasure(XFA_Attribute eAttr) const {
  return TryMeasure(eAttr, true).value_or(CXFA_Measurement());
}

float CJX_Object::GetMeasureInUnit(XFA_Attribute eAttr, XFA_Unit unit) const {
  return GetMeasure(eAttr).ToUnit(unit);
}

WideString CJX_Object::GetCData(XFA_Attribute eAttr) const {
  return TryCData(eAttr, true).value_or(WideString());
}

void CJX_Object::SetCData(XFA_Attribute eAttr, const WideString& wsValue) {
  return SetCDataImpl(eAttr, wsValue, false, false);
}

void CJX_Object::SetCDataImpl(XFA_Attribute eAttr,
                              const WideString& wsValue,
                              bool bNotify,
                              bool bScriptModify) {
  CXFA_Node* xfaObj = GetXFANode();
  uint32_t key = GetMapKey_Element(xfaObj->GetElementType(), eAttr);
  OnChanging(eAttr, bNotify);
  if (eAttr == XFA_Attribute::Value) {
    WideString* pClone = new WideString(wsValue);
    SetUserData(key, pClone, &deleteWideStringCallBack);
  } else {
    SetMapModuleString(key, wsValue.AsStringView());
    if (eAttr == XFA_Attribute::Name)
      xfaObj->UpdateNameHash();
  }
  OnChanged(eAttr, bNotify, bScriptModify);

  if (!xfaObj->IsNeedSavingXMLNode() || eAttr == XFA_Attribute::QualifiedName ||
      eAttr == XFA_Attribute::BindingNode) {
    return;
  }

  if (eAttr == XFA_Attribute::Name &&
      (xfaObj->GetElementType() == XFA_Element::DataValue ||
       xfaObj->GetElementType() == XFA_Element::DataGroup)) {
    return;
  }

  if (eAttr == XFA_Attribute::Value) {
    xfaObj->SetToXML(wsValue);
    return;
  }

  WideString wsAttrName = WideString::FromASCII(XFA_AttributeToName(eAttr));
  if (eAttr == XFA_Attribute::ContentType)
    wsAttrName = L"xfa:" + wsAttrName;

  CFX_XMLElement* elem = ToXMLElement(xfaObj->GetXMLMappingNode());
  elem->SetAttribute(wsAttrName, wsValue);
  return;
}

void CJX_Object::SetAttributeValue(const WideString& wsValue,
                                   const WideString& wsXMLValue) {
  SetAttributeValueImpl(wsValue, wsXMLValue, false, false);
}

void CJX_Object::SetAttributeValueImpl(const WideString& wsValue,
                                       const WideString& wsXMLValue,
                                       bool bNotify,
                                       bool bScriptModify) {
  auto* xfaObj = GetXFANode();
  uint32_t key =
      GetMapKey_Element(xfaObj->GetElementType(), XFA_Attribute::Value);

  OnChanging(XFA_Attribute::Value, bNotify);
  SetUserData(key, new WideString(wsValue), &deleteWideStringCallBack);
  OnChanged(XFA_Attribute::Value, bNotify, bScriptModify);

  if (!xfaObj->IsNeedSavingXMLNode())
    return;

  xfaObj->SetToXML(wsXMLValue);
}

Optional<WideString> CJX_Object::TryCData(XFA_Attribute eAttr,
                                          bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  if (eAttr == XFA_Attribute::Value) {
    void* pData;
    int32_t iBytes = 0;
    WideString* pStr = nullptr;
    if (GetMapModuleBuffer(key, &pData, &iBytes) && iBytes == sizeof(void*)) {
      memcpy(&pData, pData, iBytes);
      pStr = reinterpret_cast<WideString*>(pData);
    }
    if (pStr)
      return *pStr;
  } else {
    Optional<WideString> value = GetMapModuleString(key);
    if (value.has_value())
      return value;
  }
  if (!bUseDefault)
    return {};
  return GetXFANode()->GetDefaultCData(eAttr);
}

CFX_XMLElement* CJX_Object::SetValue(XFA_Attribute eAttr,
                                     int32_t value,
                                     bool bNotify) {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  OnChanging(eAttr, bNotify);
  SetMapModuleValue(key, value);
  OnChanged(eAttr, bNotify, false);

  CXFA_Node* pNode = GetXFANode();
  return pNode->IsNeedSavingXMLNode() ? ToXMLElement(pNode->GetXMLMappingNode())
                                      : nullptr;
}

void CJX_Object::SetContent(const WideString& wsContent,
                            const WideString& wsXMLValue,
                            bool bNotify,
                            bool bScriptModify,
                            bool bSyncData) {
  CXFA_Node* pNode = nullptr;
  CXFA_Node* pBindNode = nullptr;
  switch (GetXFANode()->GetObjectType()) {
    case XFA_ObjectType::ContainerNode: {
      if (XFA_FieldIsMultiListBox(GetXFANode())) {
        CXFA_Value* pValue =
            GetOrCreateProperty<CXFA_Value>(0, XFA_Element::Value);
        if (!pValue)
          break;

        CXFA_Node* pChildValue = pValue->GetFirstChild();
        pChildValue->JSObject()->SetCData(XFA_Attribute::ContentType,
                                          L"text/xml");
        pChildValue->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                            bScriptModify, false);

        CXFA_Node* pBind = GetXFANode()->GetBindData();
        if (bSyncData && pBind) {
          std::vector<WideString> wsSaveTextArray =
              fxcrt::Split(wsContent, L'\n');
          std::vector<CXFA_Node*> valueNodes =
              pBind->GetNodeListForType(XFA_Element::DataValue);

          // Adusting node count might have side effects, do not trust that
          // we'll ever actually get there.
          size_t tries = 0;
          while (valueNodes.size() != wsSaveTextArray.size()) {
            if (++tries > 4)
              return;
            if (valueNodes.size() < wsSaveTextArray.size()) {
              size_t iAddNodes = wsSaveTextArray.size() - valueNodes.size();
              while (iAddNodes-- > 0) {
                CXFA_Node* pValueNodes =
                    pBind->CreateSamePacketNode(XFA_Element::DataValue);
                pValueNodes->JSObject()->SetCData(XFA_Attribute::Name,
                                                  L"value");
                pValueNodes->CreateXMLMappingNode();
                pBind->InsertChildAndNotify(pValueNodes, nullptr);
              }
            } else {
              size_t iDelNodes = valueNodes.size() - wsSaveTextArray.size();
              for (size_t i = 0; i < iDelNodes; ++i)
                pBind->RemoveChildAndNotify(valueNodes[i], true);
            }
            valueNodes = pBind->GetNodeListForType(XFA_Element::DataValue);
          }
          ASSERT(valueNodes.size() == wsSaveTextArray.size());
          size_t i = 0;
          for (CXFA_Node* pValueNode : valueNodes) {
            pValueNode->JSObject()->SetAttributeValue(wsSaveTextArray[i],
                                                      wsSaveTextArray[i]);
            i++;
          }
          for (auto* pArrayNode : pBind->GetBindItemsCopy()) {
            if (pArrayNode != GetXFANode()) {
              pArrayNode->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                                 bScriptModify, false);
            }
          }
        }
        break;
      }
      if (GetXFANode()->GetElementType() == XFA_Element::ExclGroup) {
        pNode = GetXFANode();
      } else {
        CXFA_Value* pValue =
            GetOrCreateProperty<CXFA_Value>(0, XFA_Element::Value);
        if (!pValue)
          break;

        CXFA_Node* pChildValue = pValue->GetFirstChild();
        ASSERT(pChildValue);
        pChildValue->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                            bScriptModify, false);
      }
      pBindNode = GetXFANode()->GetBindData();
      if (pBindNode && bSyncData) {
        pBindNode->JSObject()->SetContent(wsContent, wsXMLValue, bNotify,
                                          bScriptModify, false);
        for (auto* pArrayNode : pBindNode->GetBindItemsCopy()) {
          if (pArrayNode != GetXFANode()) {
            pArrayNode->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                               true, false);
          }
        }
      }
      pBindNode = nullptr;
      break;
    }
    case XFA_ObjectType::ContentNode: {
      WideString wsContentType;
      if (GetXFANode()->GetElementType() == XFA_Element::ExData) {
        Optional<WideString> ret =
            TryAttribute(XFA_Attribute::ContentType, false);
        if (ret)
          wsContentType = *ret;
        if (wsContentType.EqualsASCII("text/html")) {
          wsContentType.clear();
          SetAttributeByEnum(XFA_Attribute::ContentType,
                             wsContentType.AsStringView(), false);
        }
      }

      CXFA_Node* pContentRawDataNode = GetXFANode()->GetFirstChild();
      if (!pContentRawDataNode) {
        pContentRawDataNode = GetXFANode()->CreateSamePacketNode(
            wsContentType.EqualsASCII("text/xml") ? XFA_Element::Sharpxml
                                                  : XFA_Element::Sharptext);
        GetXFANode()->InsertChildAndNotify(pContentRawDataNode, nullptr);
      }
      pContentRawDataNode->JSObject()->SetContent(
          wsContent, wsXMLValue, bNotify, bScriptModify, bSyncData);
      return;
    }
    case XFA_ObjectType::NodeC:
    case XFA_ObjectType::TextNode:
      pNode = GetXFANode();
      break;
    case XFA_ObjectType::NodeV:
      pNode = GetXFANode();
      if (bSyncData && GetXFANode()->GetPacketType() == XFA_PacketType::Form) {
        CXFA_Node* pParent = GetXFANode()->GetParent();
        if (pParent) {
          pParent = pParent->GetParent();
        }
        if (pParent && pParent->GetElementType() == XFA_Element::Value) {
          pParent = pParent->GetParent();
          if (pParent && pParent->IsContainerNode()) {
            pBindNode = pParent->GetBindData();
            if (pBindNode) {
              pBindNode->JSObject()->SetContent(wsContent, wsXMLValue, bNotify,
                                                bScriptModify, false);
            }
          }
        }
      }
      break;
    default:
      if (GetXFANode()->GetElementType() == XFA_Element::DataValue) {
        pNode = GetXFANode();
        pBindNode = GetXFANode();
      }
      break;
  }
  if (!pNode)
    return;

  SetAttributeValueImpl(wsContent, wsXMLValue, bNotify, bScriptModify);
  if (pBindNode && bSyncData) {
    for (auto* pArrayNode : pBindNode->GetBindItemsCopy()) {
      pArrayNode->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                         bScriptModify, false);
    }
  }
}

WideString CJX_Object::GetContent(bool bScriptModify) {
  return TryContent(bScriptModify, true).value_or(WideString());
}

Optional<WideString> CJX_Object::TryContent(bool bScriptModify, bool bProto) {
  CXFA_Node* pNode = nullptr;
  switch (GetXFANode()->GetObjectType()) {
    case XFA_ObjectType::ContainerNode:
      if (GetXFANode()->GetElementType() == XFA_Element::ExclGroup) {
        pNode = GetXFANode();
      } else {
        CXFA_Value* pValue =
            GetXFANode()->GetChild<CXFA_Value>(0, XFA_Element::Value, false);
        if (!pValue)
          return {};

        CXFA_Node* pChildValue = pValue->GetFirstChild();
        if (pChildValue && XFA_FieldIsMultiListBox(GetXFANode())) {
          pChildValue->JSObject()->SetAttributeByEnum(
              XFA_Attribute::ContentType, L"text/xml", false);
        }
        if (pChildValue)
          return pChildValue->JSObject()->TryContent(bScriptModify, bProto);
        return {};
      }
      break;
    case XFA_ObjectType::ContentNode: {
      CXFA_Node* pContentRawDataNode = GetXFANode()->GetFirstChild();
      if (!pContentRawDataNode) {
        XFA_Element element = XFA_Element::Sharptext;
        if (GetXFANode()->GetElementType() == XFA_Element::ExData) {
          Optional<WideString> contentType =
              TryAttribute(XFA_Attribute::ContentType, false);
          if (contentType.has_value()) {
            if (contentType.value().EqualsASCII("text/html"))
              element = XFA_Element::SharpxHTML;
            else if (contentType.value().EqualsASCII("text/xml"))
              element = XFA_Element::Sharpxml;
          }
        }
        pContentRawDataNode = GetXFANode()->CreateSamePacketNode(element);
        GetXFANode()->InsertChildAndNotify(pContentRawDataNode, nullptr);
      }
      return pContentRawDataNode->JSObject()->TryContent(bScriptModify, true);
    }
    case XFA_ObjectType::NodeC:
    case XFA_ObjectType::NodeV:
    case XFA_ObjectType::TextNode:
      pNode = GetXFANode();
      FALLTHROUGH;
    default:
      if (GetXFANode()->GetElementType() == XFA_Element::DataValue)
        pNode = GetXFANode();
      break;
  }
  if (pNode) {
    if (bScriptModify) {
      CFXJSE_Engine* pScriptContext = GetDocument()->GetScriptContext();
      pScriptContext->AddNodesOfRunScript(GetXFANode());
    }
    return TryCData(XFA_Attribute::Value, false);
  }
  return {};
}

Optional<WideString> CJX_Object::TryNamespace() {
  if (GetXFANode()->IsModelNode() ||
      GetXFANode()->GetElementType() == XFA_Element::Packet) {
    CFX_XMLNode* pXMLNode = GetXFANode()->GetXMLMappingNode();
    CFX_XMLElement* element = ToXMLElement(pXMLNode);
    if (!element)
      return {};

    return element->GetNamespaceURI();
  }

  if (GetXFANode()->GetPacketType() != XFA_PacketType::Datasets)
    return GetXFANode()->GetModelNode()->JSObject()->TryNamespace();

  CFX_XMLNode* pXMLNode = GetXFANode()->GetXMLMappingNode();
  CFX_XMLElement* element = ToXMLElement(pXMLNode);
  if (!element)
    return {};

  if (GetXFANode()->GetElementType() == XFA_Element::DataValue &&
      GetEnum(XFA_Attribute::Contains) == XFA_AttributeValue::MetaData) {
    WideString wsNamespace;
    if (!XFA_FDEExtension_ResolveNamespaceQualifier(
            element, GetCData(XFA_Attribute::QualifiedName), &wsNamespace)) {
      return {};
    }
    return wsNamespace;
  }
  return element->GetNamespaceURI();
}

std::pair<CXFA_Node*, int32_t> CJX_Object::GetPropertyInternal(
    int32_t index,
    XFA_Element eProperty) const {
  return GetXFANode()->GetProperty(index, eProperty);
}

CXFA_Node* CJX_Object::GetOrCreatePropertyInternal(int32_t index,
                                                   XFA_Element eProperty) {
  return GetXFANode()->GetOrCreateProperty(index, eProperty);
}

void CJX_Object::SetUserData(
    uint32_t key,
    void* pData,
    const XFA_MAPDATABLOCKCALLBACKINFO* pCallbackInfo) {
  ASSERT(pCallbackInfo);
  SetMapModuleBuffer(key, &pData, sizeof(void*), pCallbackInfo);
}

XFA_MAPMODULEDATA* CJX_Object::CreateMapModuleData() {
  if (!map_module_data_)
    map_module_data_ = std::make_unique<XFA_MAPMODULEDATA>();
  return map_module_data_.get();
}

XFA_MAPMODULEDATA* CJX_Object::GetMapModuleData() const {
  return map_module_data_.get();
}

void CJX_Object::SetMapModuleValue(uint32_t key, int32_t value) {
  CreateMapModuleData()->m_ValueMap[key] = value;
}

Optional<int32_t> CJX_Object::GetMapModuleValue(uint32_t key) const {
  std::set<const CXFA_Node*> visited;
  for (const CXFA_Node* pNode = GetXFANode(); pNode;
       pNode = pNode->GetTemplateNodeIfExists()) {
    if (!visited.insert(pNode).second)
      break;

    XFA_MAPMODULEDATA* pModule = pNode->JSObject()->GetMapModuleData();
    if (pModule) {
      auto it = pModule->m_ValueMap.find(key);
      if (it != pModule->m_ValueMap.end())
        return it->second;
    }
    if (pNode->GetPacketType() == XFA_PacketType::Datasets)
      break;
  }
  return {};
}

Optional<WideString> CJX_Object::GetMapModuleString(uint32_t key) const {
  void* pRawValue;
  int32_t iBytes;
  if (!GetMapModuleBuffer(key, &pRawValue, &iBytes))
    return {};

  // Defensive measure: no out-of-bounds pointers even if zero length.
  int32_t iChars = iBytes / sizeof(wchar_t);
  return WideString(iChars ? static_cast<const wchar_t*>(pRawValue) : nullptr,
                    iChars);
}

void CJX_Object::SetMapModuleBuffer(
    uint32_t key,
    void* pValue,
    size_t iBytes,
    const XFA_MAPDATABLOCKCALLBACKINFO* pCallbackInfo) {
  XFA_MAPDATABLOCK*& pBuffer = CreateMapModuleData()->m_BufferMap[key];
  if (!pBuffer) {
    pBuffer = reinterpret_cast<XFA_MAPDATABLOCK*>(
        FX_Alloc(uint8_t, XFA_MAPDATABLOCK::SizeForCapacity(iBytes)));
  } else if (pBuffer->iBytes != iBytes) {
    if (pBuffer->pCallbackInfo && pBuffer->pCallbackInfo->pFree)
      pBuffer->pCallbackInfo->pFree(*(void**)pBuffer->GetData());

    pBuffer = reinterpret_cast<XFA_MAPDATABLOCK*>(FX_Realloc(
        uint8_t, pBuffer, XFA_MAPDATABLOCK::SizeForCapacity(iBytes)));
  } else if (pBuffer->pCallbackInfo && pBuffer->pCallbackInfo->pFree) {
    pBuffer->pCallbackInfo->pFree(
        *reinterpret_cast<void**>(pBuffer->GetData()));
  }
  if (!pBuffer)
    return;

  pBuffer->pCallbackInfo = pCallbackInfo;
  pBuffer->iBytes = iBytes;
  memcpy(pBuffer->GetData(), pValue, iBytes);
}

bool CJX_Object::GetMapModuleBuffer(uint32_t key,
                                    void** pValue,
                                    int32_t* pBytes) const {
  std::set<const CXFA_Node*> visited;
  XFA_MAPDATABLOCK* pBuffer = nullptr;
  for (const CXFA_Node* pNode = GetXFANode(); pNode;
       pNode = pNode->GetTemplateNodeIfExists()) {
    if (!visited.insert(pNode).second)
      break;

    XFA_MAPMODULEDATA* pModule = pNode->JSObject()->GetMapModuleData();
    if (pModule) {
      auto it = pModule->m_BufferMap.find(key);
      if (it != pModule->m_BufferMap.end()) {
        pBuffer = it->second;
        break;
      }
    }
    if (pNode->GetPacketType() == XFA_PacketType::Datasets)
      break;
  }
  if (!pBuffer)
    return false;

  *pValue = pBuffer->GetData();
  *pBytes = pBuffer->iBytes;
  return true;
}

bool CJX_Object::HasMapModuleKey(uint32_t key) {
  XFA_MAPMODULEDATA* pModule = GetMapModuleData();
  return pModule && (pdfium::Contains(pModule->m_ValueMap, key) ||
                     pdfium::Contains(pModule->m_BufferMap, key));
}

void CJX_Object::ClearMapModuleBuffer() {
  XFA_MAPMODULEDATA* pModule = GetMapModuleData();
  if (!pModule)
    return;

  for (auto& pair : pModule->m_BufferMap) {
    XFA_MAPDATABLOCK* pBuffer = pair.second;
    if (pBuffer) {
      if (pBuffer->pCallbackInfo && pBuffer->pCallbackInfo->pFree)
        pBuffer->pCallbackInfo->pFree(*(void**)pBuffer->GetData());

      FX_Free(pBuffer);
    }
  }
  pModule->m_BufferMap.clear();
  pModule->m_ValueMap.clear();
}

void CJX_Object::RemoveMapModuleKey(uint32_t key) {
  XFA_MAPMODULEDATA* pModule = GetMapModuleData();
  if (!pModule)
    return;

  auto it = pModule->m_BufferMap.find(key);
  if (it != pModule->m_BufferMap.end()) {
    XFA_MAPDATABLOCK* pBuffer = it->second;
    if (pBuffer) {
      if (pBuffer->pCallbackInfo && pBuffer->pCallbackInfo->pFree)
        pBuffer->pCallbackInfo->pFree(*(void**)pBuffer->GetData());

      FX_Free(pBuffer);
    }
    pModule->m_BufferMap.erase(it);
  }
  pModule->m_ValueMap.erase(key);
  return;
}

void CJX_Object::MergeAllData(CXFA_Object* pDstModule) {
  XFA_MAPMODULEDATA* pDstModuleData =
      ToNode(pDstModule)->JSObject()->CreateMapModuleData();
  XFA_MAPMODULEDATA* pSrcModuleData = GetMapModuleData();
  if (!pSrcModuleData)
    return;

  for (const auto& pair : pSrcModuleData->m_ValueMap)
    pDstModuleData->m_ValueMap[pair.first] = pair.second;

  for (const auto& pair : pSrcModuleData->m_BufferMap) {
    XFA_MAPDATABLOCK* pSrcBuffer = pair.second;
    XFA_MAPDATABLOCK*& pDstBuffer = pDstModuleData->m_BufferMap[pair.first];
    if (pSrcBuffer->pCallbackInfo && pSrcBuffer->pCallbackInfo->pFree &&
        !pSrcBuffer->pCallbackInfo->pCopy) {
      if (pDstBuffer) {
        pDstBuffer->pCallbackInfo->pFree(*(void**)pDstBuffer->GetData());
        pDstModuleData->m_BufferMap.erase(pair.first);
      }
      continue;
    }
    if (!pDstBuffer) {
      pDstBuffer = (XFA_MAPDATABLOCK*)FX_Alloc(
          uint8_t, XFA_MAPDATABLOCK::SizeForCapacity(pSrcBuffer->iBytes));
    } else if (pDstBuffer->iBytes != pSrcBuffer->iBytes) {
      if (pDstBuffer->pCallbackInfo && pDstBuffer->pCallbackInfo->pFree) {
        pDstBuffer->pCallbackInfo->pFree(*(void**)pDstBuffer->GetData());
      }
      pDstBuffer = (XFA_MAPDATABLOCK*)FX_Realloc(
          uint8_t, pDstBuffer,
          XFA_MAPDATABLOCK::SizeForCapacity(pSrcBuffer->iBytes));
    } else if (pDstBuffer->pCallbackInfo && pDstBuffer->pCallbackInfo->pFree) {
      pDstBuffer->pCallbackInfo->pFree(*(void**)pDstBuffer->GetData());
    }
    if (!pDstBuffer)
      continue;

    pDstBuffer->pCallbackInfo = pSrcBuffer->pCallbackInfo;
    pDstBuffer->iBytes = pSrcBuffer->iBytes;
    memcpy(pDstBuffer->GetData(), pSrcBuffer->GetData(), pSrcBuffer->iBytes);
    if (pDstBuffer->pCallbackInfo && pDstBuffer->pCallbackInfo->pCopy) {
      pDstBuffer->pCallbackInfo->pCopy(*(void**)pDstBuffer->GetData());
    }
  }
}

void CJX_Object::MoveBufferMapData(CXFA_Object* pDstModule) {
  if (!pDstModule)
    return;

  if (pDstModule->GetElementType() == GetXFAObject()->GetElementType())
    ToNode(pDstModule)->JSObject()->TakeCalcDataFrom(this);

  if (!pDstModule->IsNodeV())
    return;

  WideString wsValue = ToNode(pDstModule)->JSObject()->GetContent(false);
  WideString wsFormatValue(wsValue);
  CXFA_Node* pNode = ToNode(pDstModule)->GetContainerNode();
  if (pNode)
    wsFormatValue = pNode->GetFormatDataValue(wsValue);

  ToNode(pDstModule)
      ->JSObject()
      ->SetContent(wsValue, wsFormatValue, true, true, true);
}

void CJX_Object::MoveBufferMapData(CXFA_Object* pSrcModule,
                                   CXFA_Object* pDstModule) {
  if (!pSrcModule || !pDstModule)
    return;

  CXFA_Node* pSrcChild = ToNode(pSrcModule)->GetFirstChild();
  CXFA_Node* pDstChild = ToNode(pDstModule)->GetFirstChild();
  while (pSrcChild && pDstChild) {
    MoveBufferMapData(pSrcChild, pDstChild);

    pSrcChild = pSrcChild->GetNextSibling();
    pDstChild = pDstChild->GetNextSibling();
  }
  ToNode(pSrcModule)->JSObject()->MoveBufferMapData(pDstModule);
}

void CJX_Object::OnChanging(XFA_Attribute eAttr, bool bNotify) {
  if (!bNotify || !GetXFANode()->IsInitialized())
    return;

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (pNotify)
    pNotify->OnValueChanging(GetXFANode(), eAttr);
}

void CJX_Object::OnChanged(XFA_Attribute eAttr,
                           bool bNotify,
                           bool bScriptModify) {
  if (bNotify && GetXFANode()->IsInitialized())
    GetXFANode()->SendAttributeChangeMessage(eAttr, bScriptModify);
}

CJX_Object::CalcData* CJX_Object::GetOrCreateCalcData(cppgc::Heap* heap) {
  if (!calc_data_) {
    calc_data_ =
        cppgc::MakeGarbageCollected<CalcData>(heap->GetAllocationHandle());
  }
  return calc_data_;
}

void CJX_Object::TakeCalcDataFrom(CJX_Object* that) {
  calc_data_ = that->calc_data_;
  that->calc_data_ = nullptr;
}

void CJX_Object::ScriptAttributeString(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_Attribute eAttribute) {
  if (!bSetting) {
    pValue->SetString(GetAttributeByEnum(eAttribute).ToUTF8().AsStringView());
    return;
  }

  WideString wsValue = pValue->ToWideString();
  SetAttributeByEnum(eAttribute, wsValue.AsStringView(), true);
  if (eAttribute != XFA_Attribute::Use ||
      GetXFAObject()->GetElementType() != XFA_Element::Desc) {
    return;
  }

  CXFA_Node* pTemplateNode =
      ToNode(GetDocument()->GetXFAObject(XFA_HASHCODE_Template));
  CXFA_Subform* pSubForm =
      pTemplateNode->GetFirstChildByClass<CXFA_Subform>(XFA_Element::Subform);
  CXFA_Proto* pProtoRoot =
      pSubForm ? pSubForm->GetFirstChildByClass<CXFA_Proto>(XFA_Element::Proto)
               : nullptr;

  WideString wsID;
  WideString wsSOM;
  if (!wsValue.IsEmpty()) {
    if (wsValue[0] == '#')
      wsID = wsValue.Substr(1, wsValue.GetLength() - 1);
    else
      wsSOM = std::move(wsValue);
  }

  CXFA_Node* pProtoNode = nullptr;
  if (!wsSOM.IsEmpty()) {
    XFA_ResolveNodeRS resolveNodeRS;
    bool bRet = GetDocument()->GetScriptContext()->ResolveObjects(
        pProtoRoot, wsSOM.AsStringView(), &resolveNodeRS,
        XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Attributes |
            XFA_RESOLVENODE_Properties | XFA_RESOLVENODE_Parent |
            XFA_RESOLVENODE_Siblings,
        nullptr);
    if (bRet && resolveNodeRS.objects.front()->IsNode())
      pProtoNode = resolveNodeRS.objects.front()->AsNode();

  } else if (!wsID.IsEmpty()) {
    pProtoNode = GetDocument()->GetNodeByID(pProtoRoot, wsID.AsStringView());
  }
  if (!pProtoNode)
    return;

  CXFA_Node* pHeadChild = GetXFANode()->GetFirstChild();
  while (pHeadChild) {
    CXFA_Node* pSibling = pHeadChild->GetNextSibling();
    GetXFANode()->RemoveChildAndNotify(pHeadChild, true);
    pHeadChild = pSibling;
  }

  CXFA_Node* pProtoForm = pProtoNode->CloneTemplateToForm(true);
  pHeadChild = pProtoForm->GetFirstChild();
  while (pHeadChild) {
    CXFA_Node* pSibling = pHeadChild->GetNextSibling();
    pProtoForm->RemoveChildAndNotify(pHeadChild, true);
    GetXFANode()->InsertChildAndNotify(pHeadChild, nullptr);
    pHeadChild = pSibling;
  }
}

void CJX_Object::ScriptAttributeBool(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_Attribute eAttribute) {
  if (bSetting) {
    SetBoolean(eAttribute, pValue->ToBoolean(), true);
    return;
  }
  pValue->SetString(GetBoolean(eAttribute) ? "1" : "0");
}

void CJX_Object::ScriptAttributeInteger(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_Attribute eAttribute) {
  if (bSetting) {
    SetInteger(eAttribute, pValue->ToInteger(), true);
    return;
  }
  pValue->SetInteger(GetInteger(eAttribute));
}

void CJX_Object::ScriptSomFontColor(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  CXFA_Font* font = ToNode(object_.Get())->GetOrCreateFontIfPossible();
  if (!font)
    return;

  if (bSetting) {
    int32_t r;
    int32_t g;
    int32_t b;
    std::tie(r, g, b) = StrToRGB(pValue->ToWideString());
    FX_ARGB color = ArgbEncode(0xff, r, g, b);
    font->SetColor(color);
    return;
  }

  int32_t a;
  int32_t r;
  int32_t g;
  int32_t b;
  std::tie(a, r, g, b) = ArgbDecode(font->GetColor());
  pValue->SetString(ByteString::Format("%d,%d,%d", r, g, b).AsStringView());
}

void CJX_Object::ScriptSomFillColor(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  CXFA_Border* border = ToNode(object_.Get())->GetOrCreateBorderIfPossible();
  CXFA_Fill* borderfill = border->GetOrCreateFillIfPossible();
  if (!borderfill)
    return;

  if (bSetting) {
    int32_t r;
    int32_t g;
    int32_t b;
    std::tie(r, g, b) = StrToRGB(pValue->ToWideString());
    FX_ARGB color = ArgbEncode(0xff, r, g, b);
    borderfill->SetColor(color);
    return;
  }

  FX_ARGB color = borderfill->GetColor(false);
  int32_t a;
  int32_t r;
  int32_t g;
  int32_t b;
  std::tie(a, r, g, b) = ArgbDecode(color);
  pValue->SetString(
      WideString::Format(L"%d,%d,%d", r, g, b).ToUTF8().AsStringView());
}

void CJX_Object::ScriptSomBorderColor(CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_Attribute eAttribute) {
  CXFA_Border* border = ToNode(object_.Get())->GetOrCreateBorderIfPossible();
  int32_t iSize = border->CountEdges();
  if (bSetting) {
    int32_t r = 0;
    int32_t g = 0;
    int32_t b = 0;
    std::tie(r, g, b) = StrToRGB(pValue->ToWideString());
    FX_ARGB rgb = ArgbEncode(100, r, g, b);
    for (int32_t i = 0; i < iSize; ++i) {
      CXFA_Edge* edge = border->GetEdgeIfExists(i);
      if (edge)
        edge->SetColor(rgb);
    }

    return;
  }

  CXFA_Edge* edge = border->GetEdgeIfExists(0);
  FX_ARGB color = edge ? edge->GetColor() : CXFA_Edge::kDefaultColor;
  int32_t a;
  int32_t r;
  int32_t g;
  int32_t b;
  std::tie(a, r, g, b) = ArgbDecode(color);
  pValue->SetString(
      WideString::Format(L"%d,%d,%d", r, g, b).ToUTF8().AsStringView());
}

void CJX_Object::ScriptSomBorderWidth(CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_Attribute eAttribute) {
  CXFA_Border* border = ToNode(object_.Get())->GetOrCreateBorderIfPossible();
  if (bSetting) {
    CXFA_Edge* edge = border->GetEdgeIfExists(0);
    CXFA_Measurement thickness =
        edge ? edge->GetMSThickness() : CXFA_Measurement(0.5, XFA_Unit::Pt);
    pValue->SetString(thickness.ToString().ToUTF8().AsStringView());
    return;
  }

  if (pValue->IsEmpty())
    return;

  WideString wsThickness = pValue->ToWideString();
  for (int32_t i = 0; i < border->CountEdges(); ++i) {
    CXFA_Edge* edge = border->GetEdgeIfExists(i);
    if (edge)
      edge->SetMSThickness(CXFA_Measurement(wsThickness.AsStringView()));
  }
}

void CJX_Object::ScriptSomMessage(CFXJSE_Value* pValue,
                                  bool bSetting,
                                  XFA_SOM_MESSAGETYPE iMessageType) {
  bool bNew = false;
  CXFA_Validate* validate = ToNode(object_.Get())->GetValidateIfExists();
  if (!validate) {
    validate = ToNode(object_.Get())->GetOrCreateValidateIfPossible();
    bNew = true;
  }

  if (bSetting) {
    if (validate) {
      switch (iMessageType) {
        case XFA_SOM_ValidationMessage:
          validate->SetScriptMessageText(pValue->ToWideString());
          break;
        case XFA_SOM_FormatMessage:
          validate->SetFormatMessageText(pValue->ToWideString());
          break;
        case XFA_SOM_MandatoryMessage:
          validate->SetNullMessageText(pValue->ToWideString());
          break;
        default:
          break;
      }
    }

    if (!bNew) {
      CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
      if (!pNotify)
        return;

      pNotify->AddCalcValidate(GetXFANode());
    }
    return;
  }

  if (!validate) {
    // TODO(dsinclair): Better error message?
    ThrowInvalidPropertyException();
    return;
  }

  WideString wsMessage;
  switch (iMessageType) {
    case XFA_SOM_ValidationMessage:
      wsMessage = validate->GetScriptMessageText();
      break;
    case XFA_SOM_FormatMessage:
      wsMessage = validate->GetFormatMessageText();
      break;
    case XFA_SOM_MandatoryMessage:
      wsMessage = validate->GetNullMessageText();
      break;
    default:
      break;
  }
  pValue->SetString(wsMessage.ToUTF8().AsStringView());
}

void CJX_Object::ScriptSomValidationMessage(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_Attribute eAttribute) {
  ScriptSomMessage(pValue, bSetting, XFA_SOM_ValidationMessage);
}

void CJX_Object::ScriptSomMandatoryMessage(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_Attribute eAttribute) {
  ScriptSomMessage(pValue, bSetting, XFA_SOM_MandatoryMessage);
}

void CJX_Object::ScriptSomDefaultValue(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_Attribute /* unused */) {
  XFA_Element eType = GetXFANode()->GetElementType();

  // TODO(dsinclair): This should look through the properties on the node to see
  // if defaultValue is defined and, if so, call that one. Just have to make
  // sure that those defaultValue calls don't call back to this one ....
  if (eType == XFA_Element::Field) {
    static_cast<CJX_Field*>(this)->defaultValue(pValue, bSetting,
                                                XFA_Attribute::Unknown);
    return;
  }
  if (eType == XFA_Element::Draw) {
    static_cast<CJX_Draw*>(this)->defaultValue(pValue, bSetting,
                                               XFA_Attribute::Unknown);
    return;
  }
  if (eType == XFA_Element::Boolean) {
    static_cast<CJX_Boolean*>(this)->defaultValue(pValue, bSetting,
                                                  XFA_Attribute::Unknown);
    return;
  }

  if (bSetting) {
    WideString wsNewValue;
    if (pValue &&
        !(pValue->IsEmpty() || pValue->IsNull() || pValue->IsUndefined())) {
      wsNewValue = pValue->ToWideString();
    }

    WideString wsFormatValue(wsNewValue);
    CXFA_Node* pContainerNode = nullptr;
    if (GetXFANode()->GetPacketType() == XFA_PacketType::Datasets) {
      WideString wsPicture;
      for (auto* pFormNode : GetXFANode()->GetBindItemsCopy()) {
        if (!pFormNode || pFormNode->HasRemovedChildren())
          continue;

        pContainerNode = pFormNode->GetContainerNode();
        if (pContainerNode) {
          wsPicture =
              pContainerNode->GetPictureContent(XFA_VALUEPICTURE_DataBind);
        }
        if (!wsPicture.IsEmpty())
          break;

        pContainerNode = nullptr;
      }
    } else if (GetXFANode()->GetPacketType() == XFA_PacketType::Form) {
      pContainerNode = GetXFANode()->GetContainerNode();
    }

    if (pContainerNode)
      wsFormatValue = pContainerNode->GetFormatDataValue(wsNewValue);

    SetContent(wsNewValue, wsFormatValue, true, true, true);
    return;
  }

  WideString content = GetContent(true);
  if (content.IsEmpty() && eType != XFA_Element::Text &&
      eType != XFA_Element::SubmitUrl) {
    pValue->SetNull();
  } else if (eType == XFA_Element::Integer) {
    pValue->SetInteger(FXSYS_wtoi(content.c_str()));
  } else if (eType == XFA_Element::Float || eType == XFA_Element::Decimal) {
    CFGAS_Decimal decimal(content.AsStringView());
    pValue->SetFloat(decimal.ToFloat());
  } else {
    pValue->SetString(content.ToUTF8().AsStringView());
  }
}

void CJX_Object::ScriptSomDefaultValue_Read(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException();
    return;
  }

  WideString content = GetContent(true);
  if (content.IsEmpty()) {
    pValue->SetNull();
    return;
  }
  pValue->SetString(content.ToUTF8().AsStringView());
}

void CJX_Object::ScriptSomDataNode(CFXJSE_Value* pValue,
                                   bool bSetting,
                                   XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException();
    return;
  }

  CXFA_Node* pDataNode = GetXFANode()->GetBindData();
  if (!pDataNode) {
    pValue->SetNull();
    return;
  }

  pValue->Assign(GetDocument()->GetScriptContext()->GetOrCreateJSBindingFromMap(
      pDataNode));
}

void CJX_Object::ScriptSomMandatory(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  CXFA_Validate* validate =
      ToNode(object_.Get())->GetOrCreateValidateIfPossible();
  if (!validate)
    return;

  if (bSetting) {
    validate->SetNullTest(pValue->ToWideString());
    return;
  }

  pValue->SetString(XFA_AttributeValueToName(validate->GetNullTest()));
}

void CJX_Object::ScriptSomInstanceIndex(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_Attribute eAttribute) {
  if (!bSetting) {
    pValue->SetInteger(Subform_and_SubformSet_InstanceIndex());
    return;
  }

  int32_t iTo = pValue->ToInteger();
  int32_t iFrom = Subform_and_SubformSet_InstanceIndex();
  CXFA_Node* pManagerNode = nullptr;
  for (CXFA_Node* pNode = GetXFANode()->GetPrevSibling(); pNode;
       pNode = pNode->GetPrevSibling()) {
    if (pNode->GetElementType() == XFA_Element::InstanceManager) {
      pManagerNode = pNode;
      break;
    }
  }
  if (!pManagerNode)
    return;

  auto* mgr = static_cast<CJX_InstanceManager*>(pManagerNode->JSObject());
  mgr->MoveInstance(iTo, iFrom);
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_Node* pToInstance = pManagerNode->GetItemIfExists(iTo);
  if (pToInstance && pToInstance->GetElementType() == XFA_Element::Subform) {
    pNotify->RunSubformIndexChange(pToInstance);
  }

  CXFA_Node* pFromInstance = pManagerNode->GetItemIfExists(iFrom);
  if (pFromInstance &&
      pFromInstance->GetElementType() == XFA_Element::Subform) {
    pNotify->RunSubformIndexChange(pFromInstance);
  }
}

void CJX_Object::ScriptSubmitFormatMode(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_Attribute eAttribute) {}

CJX_Object::CalcData::CalcData() = default;

CJX_Object::CalcData::~CalcData() = default;

void CJX_Object::CalcData::Trace(cppgc::Visitor* visitor) const {
  ContainerTrace(visitor, m_Globals);
}
