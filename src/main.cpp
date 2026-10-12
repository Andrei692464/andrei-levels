// ---------------- Multiplayer local, fara cont ----------------
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#if __has_include(<Geode/ui/CCMenuItemSpriteExtra.hpp>)
    #include <Geode/ui/CCMenuItemSpriteExtra.hpp>
#elif __has_include(<Geode/ui/ButtonSprite.hpp>)
    #include <Geode/ui/ButtonSprite.hpp>
#else
    #include <cocos2d.h>
    using CCMenuItemSpriteExtra = cocos2d::CCMenuItemSprite;
#endif

using namespace geode;
using namespace geode::ui;
using namespace cocos2d;

struct MPState {
    bool connected = false;
    std::string username;
};

static MPState g_mp;

static std::string trimString(std::string value) {
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());
    while (!value.empty() && value.back() == ' ') value.pop_back();
    return value;
}

// Helper care înlocuiește Geode/ui/ButtonSprite.hpp
// pentru ca unele versiuni ale Geode nu mai au header-ul respectiv
static CCNode* createButtonNode(const char* text, const char* font = "goldFont.fnt", const char* spriteName = "GJ_button_01.png") {
    auto btn = CCSprite::create(spriteName);
    if (!btn) {
        btn = CCSprite::create();
    }

    auto label = CCLabelBMFont::create(text, font);
    label->setScale(0.7f);
    label->setPosition(ccp(
        btn->getContentSize().width * 0.5f,
        btn->getContentSize().height * 0.5f
    ));

    btn->addChild(label);
    return btn;
}

class MultiplayerPopup;

class MultiplayerLoginPopup : public Popup<> {
protected:
    TextInput* m_username = nullptr;
    CCLabelBMFont* m_status = nullptr;
    bool m_busy = false;

    bool init() {
        if (!Popup::init(320.f, 200.f, "GJ_square01.png")) return false;
        this->setTitle("Andrei123459 Multiplayer");

        m_username = TextInput::create(260.f, "Nume jucator", "bigFont.fnt");
        m_username->setCommonFilter(CommonFilter::Any);
        m_username->setMaxCharCount(20);
        m_mainLayer->addChildAtPosition(m_username, Anchor::Center, ccp(0, 25));

        m_status = CCLabelBMFont::create("Alege un nume ca sa joci online", "chatFont.fnt");
        m_status->setScale(.7f);
        m_mainLayer->addChildAtPosition(m_status, Anchor::Center, ccp(0, -15));

        auto loginNode = createButtonNode("Joaca");
        auto loginBtn = CCMenuItemSpriteExtra::create(
            loginNode, this, menu_selector(MultiplayerLoginPopup::onLogin)
        );
        m_buttonMenu->addChildAtPosition(loginBtn, Anchor::Bottom, ccp(0, 30));

        return true;
    }

    void setStatus(std::string text) {
        if (m_status) m_status->setString(text.c_str());
    }

    void onLogin(CCObject*) {
        if (m_busy) return;

        std::string name = m_username ? std::string(m_username->getString()) : "";
        name = trimString(name);

        if (name.size() < 2) {
            this->setStatus("Numele trebuie sa aiba minim 2 caractere.");
            return;
        }

        if (name.size() > 20) {
            this->setStatus("Numele este prea lung (max 20).");
            return;
        }

        m_busy = true;
        this->setStatus("Se salveaza...");

        Mod::get()->setSavedValue("mp-username", name);
        g_mp.username = name;
        g_mp.connected = true;

        m_busy = false;
        this->onClose(nullptr);

        if (auto p = MultiplayerPopup::create()) {
            p->show();
        }
    }

public:
    static MultiplayerLoginPopup* create() {
        auto ret = new MultiplayerLoginPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class MultiplayerPopup : public Popup<> {
protected:
    bool init() {
        if (!Popup::init(320.f, 220.f, "GJ_square01.png")) return false;
        this->setTitle("Andrei123459 Multiplayer");

        auto label = CCLabelBMFont::create(
            ("Bine ai venit, " + g_mp.username).c_str(),
            "chatFont.fnt"
        );
        label->setScale(.7f);
        m_mainLayer->addChildAtPosition(label, Anchor::Center, ccp(0, 20));

        auto closeNode = createButtonNode("Inchide");
        auto closeBtn = CCMenuItemSpriteExtra::create(
            closeNode, this, menu_selector(MultiplayerPopup::onClose)
        );
        m_buttonMenu->addChildAtPosition(closeBtn, Anchor::Bottom, ccp(0, 30));

        return true;
    }

public:
    static MultiplayerPopup* create() {
        auto ret = new MultiplayerPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

static void openMultiplayer() {
    auto savedName = Mod::get()->getSavedValue<std::string>("mp-username", "");

    if (savedName.empty()) {
        if (auto p = MultiplayerLoginPopup::create()) p->show();
        return;
    }

    g_mp.username = savedName;
    g_mp.connected = true;

    if (auto p = MultiplayerPopup::create()) {
        p->show();
    }
}
