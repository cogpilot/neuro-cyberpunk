#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <Context/Context.hpp>

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

    RTTI_IMPL_TYPEINFO(StringUtils);
    RTTI_IMPL_ALLOCATOR();
};
} // namespace util

RTTI_DEFINE_CLASS(util::StringUtils, { RTTI_METHOD(BuildString); });
