#pragma once
#include <neurosdk.h>

namespace neuro {

    /*
     * \brief Provides communication between AI and game.
     */
    struct NeuroSocket {
        /**
         * \brief A callback function type for responding to Neuro's actions. This should always call RespondToAction() at *some* point.
         */
        using NeuroActionProcessor = void (*)(const neurosdk_message_action_t& aAction, NeuroSocket& aSocket);

        // How many milliseconds can we poll until we should quit?
        static constexpr auto POLL_RATE_MS = 5;

        // Neuro API context
        neurosdk_context_t m_context;

        NeuroActionProcessor m_callbackFunc;

        NeuroSocket();
        ~NeuroSocket();

        /**
        * \brief Send some context on what's happening ingame to Neuro (dialogue, we got in a car/out of a car, we got in a fight, we died...).
        * \param aContext The context to be told Neuro.
        * \param aSilent Whether Neuro should react to the provided context. Default is false.
        * 
        * \return The success of the operation.
        */
        bool SendContext(std::string_view aContext, bool aSilent = false);

        /**
        * \brief Respond to an action Neuro made. To prevent retries, the action is always marked as a success.
        * \param aActionId The ID of the action Neuro made.
        * \param aMsg The message to be sent to Neuro.
        * 
        * \return The success of the operation.
        */
        bool RespondToAction(std::string_view aActionId, std::string_view aMsg);

        /**
         * \brief Poll Neuro's socket and check for new messages.
         * \return Whether or not all operations succeeded. If this returns false, you should *probably* kill the socket and create a new one.
         */
        bool Tick();

        /**
         * \brief Create Neuro context and inform Neuro of capabilities.
         * \param aProcessor The callback function for handling Neuro's actions.
         * \return Whether or not initialization succeeded.
         */
        bool Initialize(NeuroActionProcessor aProcessor);

        // Is context still OK?
        bool IsAlive();

        static void LogNeuro(neurosdk_severity_e aSeverity, char *aMsg, void* aUserData);
    };
}
