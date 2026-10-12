// remove this forward declaration:
// class MultiplayerPopup;

// Place MultiplayerPopup definition first
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

// Now define MultiplayerLoginPopup (unchanged body)
class MultiplayerLoginPopup : public Popup {
protected:
    TextInput* m_username = nullptr;
    CCLabelBMFont* m_status = nullptr;
    bool m_busy = false;

    bool init() override {
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
#if __has_include(<Geode/ui/CCMenuItemSpriteExtra.hpp>)
        auto loginBtn = CC_MENU_ITEM_EXTRA::create(
            loginNode, nullptr, nullptr, this, menu_selector(MultiplayerLoginPopup::onLogin)
        );
#elif __has_include(<Geode/ui/ButtonSprite.hpp>)
        auto loginBtn = CC_MENU_ITEM_EXTRA::create(
            loginNode, nullptr, this, menu_selector(MultiplayerLoginPopup::onLogin)
        );
#else
        auto loginBtn = CC_MENU_ITEM_EXTRA::create(
            loginNode, nullptr, nullptr, this, menu_selector(MultiplayerLoginPopup::onLogin)
        );
#endif
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

        if (auto p = MultiplayerPopup::create()) { // now valid because MultiplayerPopup is defined
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
