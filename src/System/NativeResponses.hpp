#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

namespace mod::NeuroResponses
{
/**
 * \brief Creates a JSON response for the "query_waypoints" action.
 * 
 * This is implemented in native code due to difficulties with JSON serialization in Redscript.
 */
Red::CString CreateMappinQueryResponse();
}
