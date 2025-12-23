#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <Context/Context.hpp>

#include <Shared/Raw/String/String.hpp>

using namespace Red;

namespace util
{
class StringUtils : public IScriptable
{
public:
    static CString BuildString(const DynArray<CString>& aParts, const CString& aDelimiter)
    {
        auto len = 0u;

        for (auto i = 0u; i < aParts.size; i++)
        {
            len += aParts[i].Length();
            if (i < (aParts.size - 1u))
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

        for (auto i = 0u; i < aParts.size; i++)
        {
            for (auto j : aParts[i])
            {
                retBuffer.PushBack(j);
            }

            if (i < (aParts.size - 1u))
            {
                for (auto j : aDelimiter)
                {
                    retBuffer.PushBack(j);
                }
            }
        }

        retBuffer.PushBack('\0');

        return CString(retBuffer.entries);
    }

    static CString FormatString(const CString& aTemplate, Red::Handle<text::TextParameterSet>& aParams)
    {
        shared::raw::String::UTF16String utf16Template{aTemplate};
        auto utf16Result = utf16Template.FormatWithTextParameterSet(aParams);
        auto result = utf16Result.ToUTF8();
        return result;
    }

    RTTI_IMPL_TYPEINFO(StringUtils);
    RTTI_IMPL_ALLOCATOR();
};
} // namespace util

RTTI_DEFINE_CLASS(util::StringUtils, {
    RTTI_METHOD(BuildString);
    RTTI_METHOD(FormatString);
});
