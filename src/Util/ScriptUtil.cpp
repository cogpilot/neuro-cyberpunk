#include <Context/Context.hpp>
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <Shared/Raw/String/String.hpp>
#include <Util/ScriptUtil.hpp>

using namespace Red;

CString util::StringUtils::BuildString(const DynArray<CString>& aParts, const CString& aDelimiter)
{
    auto len = 0u;

    for (auto i = 0u; i < aParts.Size(); i++)
    {
        len += aParts[i].Length();
        if (i < (aParts.Size() - 1u))
        {
            len += aDelimiter.Length();
        }
    }

    if (len == 0u)
    {
        return "";
    }

    constexpr auto WhineAboutAllocationSizes = false;

    if constexpr (WhineAboutAllocationSizes)
    {
        Context::Spew("[BuildString] Allocation size for string: {}", len + 1u);
    }

    // This is not a good way, but yeah
    DynArray<char> retBuffer{};
    retBuffer.Reserve(len + 1u);

    for (auto i = 0u; i < aParts.Size(); i++)
    {
        for (auto j : aParts[i])
        {
            retBuffer.PushBack(j);
        }

        if (i < (aParts.Size() - 1u))
        {
            for (auto j : aDelimiter)
            {
                retBuffer.PushBack(j);
            }
        }
    }

    retBuffer.PushBack('\0');

    return CString(retBuffer.Data());
}

CString util::StringUtils::FormatString(const CString& aTemplate, Red::Handle<text::TextParameterSet>& aParams)
{
    shared::raw::String::UTF16String utf16Template{aTemplate};
    auto utf16Result = utf16Template.FormatWithTextParameterSet(aParams);
    auto formatted = utf16Result.ToUTF8();

    // Note: since this is only used for quickhacks, we can filter out HTML tags to help Neuro with context :P

    DynArray<char> final{};

    auto insideTag = false;
    for (auto ch : formatted)
    {
        if (ch == '<')
        {
            insideTag = true;
            continue;
        }
        else if (ch == '>')
        {
            insideTag = false;
            continue;
        }

        if (!insideTag)
        {
            final.PushBack(ch);
        }
    }

    final.PushBack('\0');

    CString ret{final.Data()};

    return ret;
}

RTTI_DEFINE_CLASS(util::StringUtils, {
    RTTI_METHOD(BuildString);
    RTTI_METHOD(FormatString);
});
