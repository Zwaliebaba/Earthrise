#pragma once

#include <ranges>
#include <typeindex>
#include "Event.h"

class EventSubscriber;

// This is the interface for MemberFunctionHandler that each specialization will use
class HandlerFunctionBase
{
  public:
  virtual ~HandlerFunctionBase() = default;
  HandlerFunctionBase(EventSubscriber* subscriber);

  virtual void* GetInstance() = 0;
  [[nodiscard]] EventSubscriber* GetSubscriber() const;

  void Exec(Event& _evnt);

  private:
  // Implemented by MemberFunctionHandler
  virtual void Call(Event& _evnt) = 0;

  EventSubscriber* m_subscriber;
};

template <class T, class EventType> class MemberFunctionHandler : public HandlerFunctionBase
{
  public:
  using MemberFunction = void(T::*)(EventType&);

  MemberFunctionHandler(EventSubscriber* _subscriber, T* _instance, const MemberFunction _memberFunction)
    : HandlerFunctionBase(_subscriber),
    m_instance{ _instance },
    m_memberFunction{ _memberFunction }
  {}

  void* GetInstance() override { return m_instance; }

  void Call(Event& _evnt) override
  {
    // Cast event to the correct type and call member function
    (m_instance->*m_memberFunction)(static_cast<EventType&>(_evnt));
  }

  private:
  // Pointer to class instance
  T* m_instance;

  // Pointer to member function
  MemberFunction m_memberFunction;
};

class EventManager
{
  public:
  using handler_list_t = std::list<HandlerFunctionBase*>;

  template <typename T_Event> static void Publish(T_Event& _evnt)
  {
    std::scoped_lock lock(sm_sync);

    auto hlrPtr = sm_subscribers.find(typeid(T_Event));
    if (hlrPtr == sm_subscribers.end())
      return;

    for (auto& handlers : *hlrPtr->second)
    {
      handlers->Exec(_evnt);
      if (_evnt.IsConsumed())
        break;
    }
  }

  template <class T_Event, class T_Instance> static void Subscribe(T_Instance* _instance,
    void (T_Instance::* _memberFunction)(T_Event&))
  {
    handler_list_t* handlers;

    std::scoped_lock lock(sm_sync);

    if (auto handlerPtr = sm_subscribers.find(typeid(T_Event)); handlerPtr == sm_subscribers.end())
    {
      handlers = NEW handler_list_t();
      sm_subscribers.emplace(typeid(T_Event), handlers);
    }
    else
      handlers = handlerPtr->second.get();

    handlers->emplace_back(NEW MemberFunctionHandler<T_Instance, T_Event>(_instance, static_cast<T_Instance*>(_instance),
      _memberFunction));
  }

  // Unsubscribe an instance from a single event type.
  template <class T_Event, class T_Instance> static void Unsubscribe(T_Instance* _instance)
  {
    std::scoped_lock lock(sm_sync);

    if (handler_list_t* handlers = sm_subscribers[typeid(T_Event)]; handlers != nullptr)
      handlers->remove_if([&](HandlerFunctionBase* x) { return (static_cast<T_Instance*>(x->GetInstance()) == _instance); });
  }

  // Unsubscribe an instance from all event types.
  template <class T_Instance> static void UnsubscribeAll(T_Instance* instance)
  {
    std::scoped_lock lock(sm_sync);

    for (auto& sub : sm_subscribers | std::views::values)
      sub->remove_if([&](auto& x) { return (static_cast<T_Instance*>(x->GetInstance()) == instance); });
  }


  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static void AddEventProcessor(WNDPROC _driver);
  static void RemoveEventProcessor(WNDPROC _driver);

  protected:
  inline static std::map<std::type_index, std::unique_ptr<handler_list_t>> sm_subscribers;
  inline static std::vector<WNDPROC> sm_eventprocs;
  inline static std::mutex sm_sync;
};

// Class that keeps track of event subscriptions and can auto-unsubscribe
// itself in the destructor
class EventSubscriber
{
  public:
  friend class EventManager;
  friend class HandlerFunctionBase;

  virtual ~EventSubscriber();

  void UnsubscribeFromAllEvents();
};

