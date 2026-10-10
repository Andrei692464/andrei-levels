#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>

using namespace geode::prelude;

class $modify(AndreiCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setID("andrei-levels-menu"_spr);
        menu->setPosition({winSize.width / 2.f, winSize.height - 22.f});

        auto spr = ButtonSprite::create(
            "Andrei123459 Levels", 200, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .6f
        );
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(AndreiCreatorLayer::onAndreiLevels)
        );
        btn->setID("andrei-levels-button"_spr);
        menu->addChild(btn);

        this->addChild(menu, 10);
        return true;
    }

    void onAndreiLevels(CCObject*) {
        auto userID = Mod::get()->getSettingValue<int64_t>("user-id");
        if (userID <= 0) {
            FLAlertLayer::create(
                "Setare lipsa",
                "Pune <cy>User ID</c>-ul in setarile modului.",
                "OK"
            )->show();
            return;
        }

        auto search = GJSearchObject::create(SearchType::UsersLevels, std::to_string(userID));
        auto scene = LevelBrowserLayer::scene(search);
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.5f, scene));
    }
};
