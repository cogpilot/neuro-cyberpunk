#pragma once
#include <neurosdk.h>

#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <RED4ext/StringView.hpp>

namespace neuro {

    /*
     * \brief Provides communication between AI and game.
     */
    struct NeuroSocket {
        // How many milliseconds can we poll until we should quit?
        static constexpr auto PollRateMs = 5;

        // Neuro API context
        neurosdk_context_t m_context;

        NeuroSocket();
        ~NeuroSocket();

        /**
        * \brief Send some context on what's happening ingame to Neuro (dialogue, we got in a car/out of a car, we got in a fight, we died...).
        * \param aContext The context to be told Neuro.
        * \param aSilent Whether Neuro should react to the provided context. Default is false.
        * 
        * \return The success of the operation.
        */
        bool SendContext(Red::StringView aContext, bool aSilent = false);

        /**
        * \brief Respond to an action Neuro made. To prevent retries, the action is marked as a success by default.
        * \param aActionId The ID of the action Neuro made.
        * \param aMsg The message to be sent to Neuro.
        * \param aSuccess Whether the action was successful and thus shouldn't be retried.
        * 
        * \return The success of the operation.
        */
        bool RespondToAction(Red::StringView aActionId, Red::StringView aMsg, bool aSuccess = true);

        /**
         * \brief Send a message telling Neuro to do something.
         * 
         * \param aActionName The name of the action to take. We don't support multiple actions for this.
         * \param aQuery A short description for what Neuro should do.
         * \param aState A string describing the state of the game.
         * 
         * \return The success of the operation.
         */
        bool SendForcedAction(Red::StringView aActionName, Red::StringView aQuery, Red::StringView aState, Red::StringView aPriority = "medium");

        /**
         * \brief Poll Neuro's socket and check for new messages.
         * 
         * \param aClosure The processor function. NOTE: The strings in the action message are owned by the message and should be copied before use.
         * 
         * \return Whether or not all operations succeeded. If this returns false, you should *probably* kill the socket and create a new one.
         */
        template<typename L>
            requires Red::Detail::IsClosure<L, void, const neurosdk_message_action_t&>
        bool Tick(L&& aClosure)
        {
            if (!IsAlive())
            {
                return false;
            }

            neurosdk_message_t* messageQueue{};
            int messageCount{};
            if (const auto status = neurosdk_context_poll(&m_context, &messageQueue, &messageCount);
                status != NeuroSDK_None)
            {
                return false;
            }

            for (auto i = 0; i < messageCount; i++)
            {
                if (messageQueue[i].kind == NeuroSDK_MessageKind_Action)
                {
                    aClosure(messageQueue[i].value.action);
                    // m_callbackFunc(messageQueue[i].value.action, *this);
                    neurosdk_message_destroy(&messageQueue[i]);
                }
            }

            return true;
        }

        /**
         * \brief Create Neuro context and inform Neuro of capabilities.
         * \param aProcessor The callback function for handling Neuro's actions.
         * \return Whether or not initialization succeeded.
         */
        bool Initialize();

        // Is context still OK?
        bool IsAlive();

        static void LogNeuro(neurosdk_severity_e aSeverity, char *aMsg, void* aUserData);
    };
}
