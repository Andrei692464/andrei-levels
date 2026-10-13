#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

#if __has_include(<Geode/ui/CCMenuItemSpriteExtra.hpp>)
#include <Geode/ui/CCMenuItemSpriteExtra.hpp>
#define CC_MENU_ITEM_EXTRA CCMenuItemSpriteExtra
#elif __has_include(<Geode/ui/ButtonSprite.hpp>)
#include <Geode/ui/ButtonSprite.hpp>
#define CC_MENU_ITEM_EXTRA CCMenuItemSpriteExtra
#else
#define CC_MENU_ITEM_EXTRA CCMenuItemSpriteExtra
#endif

struct MultiplayerState {
    std::string username;
    bool connected = false;
};

static MultiplayerState g_mp;

static constexpr char const* kMultiplayerUsername = "ANDREI123459 MULTYPLAYER";

static CCNode* createButtonNode(char const* label) {
    return ButtonSprite::create(label, 120.f, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .8f);
}

static void ensureMultiplayerLoggedIn() {
    g_mp.username = kMultiplayerUsername;
    g_mp.connected = true;
    Mod::get()->setSavedValue("mp-username", std::string(kMultiplayerUsername));
}

static void showMultiplayerMenu();

class MultiplayerPopup : public Popup {
protected:
    bool init() override {
        if (!Popup::init(320.f, 220.f, "GJ_square01.png")) return false;
        this->setTitle("Andrei123459 Multiplayer");

        auto label = CCLabelBMFont::create(
            ("Bine ai venit, " + g_mp.username).c_str(),
            "chatFont.fnt"
        );
        label->setScale(.7f);
        m_mainLayer->addChildAtPosition(label, Anchor::Center, ccp(0, 20));

        auto status = CCLabelBMFont::create("Esti conectat la multiplayer.", "chatFont.fnt");
        status->setScale(.55f);
        m_mainLayer->addChildAtPosition(status, Anchor::Center, ccp(0, -10));

        auto closeNode = createButtonNode("Inchide");
#if __has_include(<Geode/ui/CCMenuItemSpriteExtra.hpp>)
        auto closeBtn = CC_MENU_ITEM_EXTRA::create(
            closeNode, nullptr, nullptr, this, menu_selector(MultiplayerPopup::onClose)
        );
#elif __has_include(<Geode/ui/ButtonSprite.hpp>)
        auto closeBtn = CC_MENU_ITEM_EXTRA::create(
            closeNode, nullptr, this, menu_selector(MultiplayerPopup::onClose)
        );
#else
        auto closeBtn = CC_MENU_ITEM_EXTRA::create(
            closeNode, nullptr, nullptr, this, menu_selector(MultiplayerPopup::onClose)
        );
#endif
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

static void showMultiplayerMenu() {
    ensureMultiplayerLoggedIn();
    if (auto popup = MultiplayerPopup::create()) {
        popup->show();
    }
}

$execute {
    ensureMultiplayerLoggedIn();
}

class $modify(AndreiMultiplayerMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto spr = ButtonSprite::create(
            "Andrei123459 Multiplayer",
            200.f,
            true,
            "goldFont.fnt",
            "GJ_button_01.png",
            30.f,
            .55f
        );
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(AndreiMultiplayerMenuLayer::onMultiplayer)
        );
        btn->setID("andrei-multiplayer-button"_spr);

        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }

        return true;
    }

    void onMultiplayer(CCObject*) {
        showMultiplayerMenu();
    }
};
