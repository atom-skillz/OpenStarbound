#include "StarInput.hpp"
#include "StarAssets.hpp"
#include "StarRoot.hpp"
#include "StarJsonExtra.hpp"

namespace Star {

const char* InputBindingConfigRoot = "modBindings";

BiMap<Key, KeyMod> const KeysToMods{
  {Key::LShift, KeyMod::LShift},
  {Key::RShift, KeyMod::RShift},
  {Key::LCtrl, KeyMod::LCtrl},
  {Key::RCtrl, KeyMod::RCtrl},
  {Key::LAlt, KeyMod::LAlt},
  {Key::RAlt, KeyMod::RAlt},
  {Key::LGui, KeyMod::LGui},
  {Key::RGui, KeyMod::RGui},
  {Key::AltGr, KeyMod::AltGr},
  {Key::ScrollLock, KeyMod::Scroll}
};

const KeyMod KeyModOptional = KeyMod::Num | KeyMod::Caps | KeyMod::Scroll;

inline bool compareKeyModLenient(KeyMod input, KeyMod test) {
	input |= KeyModOptional;
	test |= KeyModOptional;
	return (test & input) == test;
}

inline bool compareKeyMod(KeyMod input, KeyMod test) {
	return (input | (KeyModOptional & ~test)) == (test | KeyModOptional);
}

Json keyModsToJson(KeyMod mod) {
  JsonArray array;
  
  if ((bool)(mod & KeyMod::LShift)) array.emplace_back("LShift");
  if ((bool)(mod & KeyMod::RShift)) array.emplace_back("RShift");
  if ((bool)(mod & KeyMod::LCtrl )) array.emplace_back("LCtrl" );
  if ((bool)(mod & KeyMod::RCtrl )) array.emplace_back("RCtrl" );
  if ((bool)(mod & KeyMod::LAlt  )) array.emplace_back("LAlt"  );
  if ((bool)(mod & KeyMod::RAlt  )) array.emplace_back("RAlt"  );
  if ((bool)(mod & KeyMod::LGui  )) array.emplace_back("LGui"  );
  if ((bool)(mod & KeyMod::RGui  )) array.emplace_back("RGui"  );
  if ((bool)(mod & KeyMod::Num   )) array.emplace_back("Num"   );
  if ((bool)(mod & KeyMod::Caps  )) array.emplace_back("Caps"  );
  if ((bool)(mod & KeyMod::AltGr )) array.emplace_back("AltGr" );
  if ((bool)(mod & KeyMod::Scroll)) array.emplace_back("Scroll");

  return array.empty() ? Json() : move(array);
}

// Optional pointer argument to output calculated priority
KeyMod keyModsFromJson(Json const& json, uint8_t* priority = nullptr) {
  KeyMod mod = KeyMod::NoMod;
  if (!json.isType(Json::Type::Array))
    return mod;

  uint8_t modPriority = 0;
  for (Json const& jMod : json.toArray()) {
    KeyMod changedMod = mod | KeyModNames.getLeft(jMod.toString());
    if (mod != changedMod) {
      mod = changedMod;
      ++modPriority;
    }
  }
  if (priority)
    *priority = modPriority;

  return mod;
}

size_t hash<InputVariant>::operator()(InputVariant const& v) const {
  size_t indexHash = hashOf(v.typeIndex());
  if (auto key = v.ptr<Key>())
    hashCombine(indexHash, hashOf(*key));
  else if (auto mButton = v.ptr<MouseButton>())
    hashCombine(indexHash, hashOf(*mButton));
  else if (auto cButton = v.ptr<ControllerButton>())
    hashCombine(indexHash, hashOf(*cButton));

  return indexHash;
}

Json Input::inputEventToJson(InputEvent const& input) {
  String type;
  Json data;

  if (auto keyDown = input.ptr<KeyDownEvent>()) {
    type = "KeyDown";
    data = JsonObject{
      {"key", KeyNames.getRight(keyDown->key)},
      {"mods", keyModsToJson(keyDown->mods)}
    };
  } else if (auto keyUp = input.ptr<KeyUpEvent>()) {
    type = "KeyUp";
    data = JsonObject{
      {"key", KeyNames.getRight(keyUp->key)}
    };
  } else if (auto mouseDown = input.ptr<MouseButtonDownEvent>()) {
    type = "MouseButtonDown";
    data = JsonObject{
      {"mouseButton", MouseButtonNames.getRight(mouseDown->mouseButton)},
      {"mousePosition", jsonFromVec2I(mouseDown->mousePosition)}
    };
  } else if (auto mouseUp = input.ptr<MouseButtonUpEvent>()) {
    type = "MouseButtonUp";
    data = JsonObject{
      {"mouseButton", MouseButtonNames.getRight(mouseUp->mouseButton)},
      {"mousePosition", jsonFromVec2I(mouseUp->mousePosition)}
    };
  } else if (auto mouseWheel = input.ptr<MouseWheelEvent>()) {
    type = "MouseWheel";
    data = JsonObject{
      {"mouseWheel", MouseWheelNames.getRight(mouseWheel->mouseWheel)},
      {"mousePosition", jsonFromVec2I(mouseWheel->mousePosition)}
    };
  } else if (auto mouseMove = input.ptr<MouseMoveEvent>()) {
    type = "MouseMove";
    data = JsonObject{
      {"mouseMove", jsonFromVec2I(mouseMove->mouseMove)},
      {"mousePosition", jsonFromVec2I(mouseMove->mousePosition)}
    };
  }

  if (data) {
    return JsonObject{
      {"type", type},
      {"data", data}
    };
  }

  return data;
}

Input::Bind Input::bindFromJson(Json const& json) {
  Bind bind;
  if (json.isNull())
    return bind;

  String type = json.getString("type");
  Json value = json.get("value", {});

  if (type == "key") {
    KeyBind keyBind;
    keyBind.key = KeyNames.getLeft(value.toString());
    keyBind.mods = keyModsFromJson(json.getArray("mods", {}), &keyBind.priority);
    bind = move(keyBind);
  }
  else if (type == "mouse") {
    MouseBind mouseBind;
    mouseBind.button = MouseButtonNames.getLeft(value.toString());
    mouseBind.mods = keyModsFromJson(json.getArray("mods", {}), &mouseBind.priority);
    bind = move(mouseBind);
  }
  else if (type == "controller") {
    ControllerBind controllerBind;
    controllerBind.button = ControllerButtonNames.getLeft(value.toString());
    controllerBind.controller = json.getUInt("controller", 0);
    bind = move(controllerBind);
  }

  return bind;
}

Json Input::bindToJson(Bind const& bind) {
  if (auto keyBind = bind.ptr<KeyBind>()) {
    auto obj = JsonObject{
      {"type", "key"},
      {"value", KeyNames.getRight(keyBind->key)}
    }; // don't want empty mods to exist as null entry
    if (auto mods = keyModsToJson(keyBind->mods))
      obj.emplace("mods", move(mods));
    return move(obj);
  }
  else if (auto mouseBind = bind.ptr<MouseBind>()) {
    auto obj = JsonObject{
      {"type", "mouse"},
      {"value", MouseButtonNames.getRight(mouseBind->button)}
    };
    if (auto mods = keyModsToJson(mouseBind->mods))
      obj.emplace("mods", move(mods));
    return move(obj);
  }
  else if (auto controllerBind = bind.ptr<ControllerBind>()) {
    return JsonObject{
      {"type", "controller"},
      {"value", ControllerButtonNames.getRight(controllerBind->button)}
    };
  }

  return Json();
}

Input::BindEntry::BindEntry(String entryId, Json const& config, BindCategory const& parentCategory) {
  category = &parentCategory;
  id = entryId;
  name = config.getString("name", id);

  for (Json const& jBind : config.getArray("default", {})) {
    try
      { defaultBinds.emplace_back(bindFromJson(jBind)); }
    catch (JsonException const& e)
      { Logger::error("Binds: Error loading default bind in {}.{}: {}", parentCategory.id, id, e.what()); }
  }
}

void Input::BindEntry::updated() {
  auto config = Root::singleton().configuration();

  JsonArray array;
  for (auto const& bind : customBinds)
    array.emplace_back(bindToJson(bind));

  if (!config->get(InputBindingConfigRoot).isType(Json::Type::Object))
    config->set(InputBindingConfigRoot, JsonObject());

  String path = strf("{}.{}", InputBindingConfigRoot, category->id);
  if (!config->getPath(path).isType(Json::Type::Object)) {
    config->setPath(path, JsonObject{
      { id, move(array) }
    });
  }
  else {
    path = strf("{}.{}", path, id);
    config->setPath(path, array);
  }

  Input::singleton().rebuildMappings();
}

Input::BindRef::BindRef(BindEntry& bindEntry, KeyBind& keyBind) {
  entry = &bindEntry;
  priority = keyBind.priority;
  mods = keyBind.mods;
}

Input::BindRef::BindRef(BindEntry& bindEntry, MouseBind& mouseBind) {
  entry = &bindEntry;
  priority = mouseBind.priority;
  mods = mouseBind.mods;
}

Input::BindRef::BindRef(BindEntry& bindEntry) {
  entry = &bindEntry;
  priority = 0;
  mods = KeyMod::NoMod;
}

Input::BindCategory::BindCategory(String categoryId, Json const& categoryConfig) {
  id = categoryId;
  config = categoryConfig;
  name = config.getString("name", id);

  ConfigurationPtr userConfig = Root::singletonPtr()->configuration();
  auto userBindings = userConfig->get(InputBindingConfigRoot);

  for (auto& pair : config.getObject("binds", {})) {
    String const& bindId = pair.first;
    Json const& bindConfig = pair.second;
    if (!bindConfig.isType(Json::Type::Object))
      continue;

    BindEntry& entry = entries.try_emplace(bindId, bindId, bindConfig, *this).first->second;

    if (userBindings.isType(Json::Type::Object)) {
      for (auto& jBind : userBindings.queryArray(strf("{}.{}", id, bindId), {})) {
        try
          { entry.customBinds.emplace_back(bindFromJson(jBind)); }
        catch (JsonException const& e)
          { Logger::error("Binds: Error loading user bind in {}.{}: {}", id, bindId, e.what()); }
      }
    }

    if (entry.customBinds.empty())
      entry.customBinds = entry.defaultBinds;
  }
}

List<Input::BindEntry*> Input::filterBindEntries(List<Input::BindRef> const& binds, KeyMod mods) const {
  uint8_t maxPriority = 0;
  List<BindEntry*> result{};
  for (const BindRef& bind : binds) {
    if (bind.priority < maxPriority)
      break;
    else if (compareKeyModLenient(mods, bind.mods)) {
      maxPriority = bind.priority;
      result.emplace_back(bind.entry);
    }
  }
  return result;
}

Input::BindEntry* Input::bindEntryPtr(String const& categoryId, String const& bindId) {
  if (auto category = m_bindCategories.ptr(categoryId)) {
    if (auto entry = category->entries.ptr(bindId)) {
      return entry;
    }
  }
  
  return nullptr;
}

Input::BindEntry& Input::bindEntry(String const& categoryId, String const& bindId) {
  if (auto ptr = bindEntryPtr(categoryId, bindId))
    return *ptr;
  else
    throw InputException::format("Could not find bind entry {}.{}", categoryId, bindId);
}

Input::InputState* Input::bindStatePtr(String const& categoryId, String const& bindId) {
  if (auto ptr = bindEntryPtr(categoryId, bindId))
    return m_bindStates.ptr(ptr);
  else
    return nullptr;
}

Input* Input::s_singleton;

Input* Input::singletonPtr() {
  return s_singleton;
}

Input& Input::singleton() {
  if (!s_singleton)
    throw InputException("Input::singleton() called with no Input instance available");
  else
    return *s_singleton;
}

Input::Input() {
  if (s_singleton)
    throw InputException("Singleton Input has been constructed twice");

  s_singleton = this;

  m_pressedMods = KeyMod::NoMod;

  reload();

  m_rootReloadListener = make_shared<CallbackListener>([&]() {
    reload();
  });

  Root::singletonPtr()->registerReloadListener(m_rootReloadListener);
}

Input::~Input() {
  s_singleton = nullptr;
}

List<std::pair<InputEvent, bool>> const& Input::inputEventsThisFrame() const {
  return m_inputEvents;
}



void Input::reset() {
  m_inputEvents.clear();
  auto eraseCond = [](auto& p) {
    if (p.second.held)
      p.second.reset();
    return !p.second.held;
  };

  eraseWhere(m_keyStates,        eraseCond);
  eraseWhere(m_mouseStates,      eraseCond);
  eraseWhere(m_controllerStates, eraseCond);
  eraseWhere(m_bindStates,       eraseCond);
}

void Input::update() {
  reset();
}

bool Input::handleInput(InputEvent const& input, bool gameProcessed) {
  m_inputEvents.emplace_back(input, gameProcessed);
  if (auto keyDown = input.ptr<KeyDownEvent>()) {
    auto keyToMod = KeysToMods.rightPtr(keyDown->key);
    if (keyToMod)
      m_pressedMods |= *keyToMod;

    if (!gameProcessed && !m_textInputActive) {
      auto& state = m_keyStates[keyDown->key];
      if (keyToMod)
        state.mods |= *keyToMod;
      state.press();
      
      if (auto binds = m_bindMappings.ptr(keyDown->key)) {
        for (auto bind : filterBindEntries(*binds, keyDown->mods))
          m_bindStates[bind].press();
      }
    }
  } else if (auto keyUp = input.ptr<KeyUpEvent>()) {
    auto keyToMod = KeysToMods.rightPtr(keyUp->key);
    if (keyToMod)
      m_pressedMods &= ~*keyToMod;

    // We need to be able to release input even when gameProcessed is true, but only if it's already down.
    if (auto state = m_keyStates.ptr(keyUp->key)) {
      if (keyToMod)
        state->mods &= ~*keyToMod;
      state->release();
    }

    if (auto binds = m_bindMappings.ptr(keyUp->key)) {
      for (auto& bind : *binds) {
        if (auto state = m_bindStates.ptr(bind.entry))
          state->release();
      }
    }
  } else if (auto mouseDown = input.ptr<MouseButtonDownEvent>()) {
    if (!gameProcessed) {
      auto& state = m_mouseStates[mouseDown->mouseButton];
      state.pressPositions.append(mouseDown->mousePosition);
      state.press();

      if (auto binds = m_bindMappings.ptr(mouseDown->mouseButton)) {
        for (auto bind : filterBindEntries(*binds, m_pressedMods))
          m_bindStates[bind].press();
      }
    }
  } else if (auto mouseUp = input.ptr<MouseButtonUpEvent>()) {
    if (auto state = m_mouseStates.ptr(mouseUp->mouseButton)) {
      state->releasePositions.append(mouseUp->mousePosition);
      state->release();
    }

    if (auto binds = m_bindMappings.ptr(mouseUp->mouseButton)) {
      for (auto& bind : *binds) {
        if (auto state = m_bindStates.ptr(bind.entry))
          state->release();
      }
    }
  }

  return false;
}

void Input::rebuildMappings() {
  reset();
  m_bindMappings.clear();

  for (auto& category : m_bindCategories) {
    for (auto& pair : category.second.entries) {
      auto& entry = pair.second;
      for (auto& bind : entry.customBinds) {
        if (auto keyBind = bind.ptr<KeyBind>())
          m_bindMappings[keyBind->key].emplace_back(entry, *keyBind);
        if (auto mouseBind = bind.ptr<MouseBind>())
          m_bindMappings[mouseBind->button].emplace_back(entry, *mouseBind);
        if (auto controllerBind = bind.ptr<ControllerBind>())
          m_bindMappings[controllerBind->button].emplace_back(entry);
      }
    }
  }

  for (auto& pair : m_bindMappings) {
    pair.second.sort([](BindRef const& a, BindRef const& b)
      { return a.priority > b.priority; });
  }
}

void Input::reload() {;
  m_bindCategories.clear();

  auto assets = Root::singleton().assets();

  for (auto& bindPath : assets->scanExtension("binds")) {
    for (auto& pair : assets->json(bindPath).toObject()) {
      String const& categoryId = pair.first;
      Json const& categoryConfig = pair.second;
      if (!categoryConfig.isType(Json::Type::Object))
        continue;

      m_bindCategories.try_emplace(categoryId, categoryId, categoryConfig);
    }
  }

  size_t count = 0;
  for (auto& pair : m_bindCategories)
    count += pair.second.entries.size();

  Logger::info("Binds: Loaded {} bind{}", count, count == 1 ? "" : "s");

  rebuildMappings();
}

void Input::setTextInputActive(bool active) {
  m_textInputActive = active;
}

Maybe<unsigned> Input::bindDown(String const& categoryId, String const& bindId) {
  if (auto state = bindStatePtr(categoryId, bindId)) {
    if (state->presses)
      return state->presses;
  }
  return {};
}

bool Input::bindHeld(String const& categoryId, String const& bindId) {
  if (auto state = bindStatePtr(categoryId, bindId))
    return state->held;
  else
    return false;
}

Maybe<unsigned> Input::bindUp(String const& categoryId, String const& bindId) {
  if (auto state = bindStatePtr(categoryId, bindId)) {
    if (state->releases)
      return state->releases;
  }
  return {};
}

Maybe<unsigned> Input::keyDown(Key key, Maybe<KeyMod> keyMod) {
  if (auto state = m_keyStates.ptr(key)) {
    if (state->presses && (!keyMod || compareKeyMod(*keyMod, state->mods)))
      return state->presses;
  }
  return {};
}

bool Input::keyHeld(Key key) {
  auto state = m_keyStates.ptr(key);
  return state && state->held;
}

Maybe<unsigned> Input::keyUp(Key key) {
  if (auto state = m_keyStates.ptr(key)) {
    if (state->releases)
      return state->releases;
  }
  return {};
}

Maybe<List<Vec2I>> Input::mouseDown(MouseButton button) {
  if (auto state = m_mouseStates.ptr(button)) {
    if (state->presses)
      return state->pressPositions;
  }
  return {};
}

bool Input::mouseHeld(MouseButton button) {
  auto state = m_mouseStates.ptr(button);
  return state && state->held;
}

Maybe<List<Vec2I>> Input::mouseUp(MouseButton button) {
  if (auto state = m_mouseStates.ptr(button)) {
    if (state->releases)
      return state->releasePositions;
  }
  return {};
}

void Input::resetBinds(String const& categoryId, String const& bindId) {
  auto& entry = bindEntry(categoryId, bindId);

  entry.customBinds = entry.defaultBinds;
  entry.updated();
}

Json Input::getDefaultBinds(String const& categoryId, String const& bindId) {
  JsonArray array;
  for (Bind const& bind : bindEntry(categoryId, bindId).defaultBinds)
    array.emplace_back(bindToJson(bind));

  return move(array);
}

Json Input::getBinds(String const& categoryId, String const& bindId) {
  JsonArray array;
  for (Bind const& bind : bindEntry(categoryId, bindId).customBinds)
    array.emplace_back(bindToJson(bind));

  return move(array);
}

void Input::setBinds(String const& categoryId, String const& bindId, Json const& jBinds) {
  auto& entry = bindEntry(categoryId, bindId);

  List<Bind> binds;
  for (Json const& jBind : jBinds.toArray())
    binds.emplace_back(bindFromJson(jBind));

  entry.customBinds = move(binds);
  entry.updated();
}

}