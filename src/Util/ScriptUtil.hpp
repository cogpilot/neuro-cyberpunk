#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/text/TextParameterSet.hpp>

namespace util
{
class StringUtils : public Red::IScriptable
{
public:
    static Red::CString BuildString(const Red::DynArray<Red::CString>& aParts, const Red::CString& aDelimiter);
    static Red::CString FormatString(const Red::CString& aTemplate, Red::Handle<Red::text::TextParameterSet>& aParams);

    RTTI_IMPL_TYPEINFO(StringUtils);
    RTTI_IMPL_ALLOCATOR();
};
} // namespace util