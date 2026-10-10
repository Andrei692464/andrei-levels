#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>

using namespace geode::prelude;

static CCNode* makeAndreiSprite(CCSize size) {
    auto spr = ButtonSprite::create(
        "", (int)size.width, true, "goldFont.fnt", "GJ_button_01.png", size.height, 1.f
    );
    auto cs = spr->getContentSize();

    auto icon = SimplePlayer::create(1);
    icon->setScale(cs.height * 0.45f / std::max(icon->getContentSize().height, 1.f));
    icon->setPosition({cs.width / 2.f, cs.height * 0.66f});
    spr->addChild(icon);

    auto name = CCLabelBMFont::create("Andrei123459", "goldFont.fnt");
    name->limitLabelWidth(cs.width * 0.88f, .7f, .1f);
    name->setPosition({cs.width / 2.f, cs.height * 0.32f});
    spr->addChild(name);

    auto lvls = CCLabelBMFont::create("Levels", "goldFont.fnt");
    lvls->limitLabelWidth(cs.width * 0.88f, .7f, .1f);
    lvls->setPosition({cs.width / 2.f, cs.height * 0.15f});
    spr->addChild(lvls);

    return spr;
}

class $modify(AndreiCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        auto menu = this->getChildByID("creator-buttons-menu");
        if (menu && menu->getChildrenCount() > 0) {
            auto first = static_cast<CCNode*>(menu->getChildren()->objectAtIndex(0));
            CCSize size = first->getContentSize();
            if (size.width < 20.f || size.height < 20.f) size = CCSize(90.f, 90.f);

            auto btn = CCMenuItemSpriteExtra::create(
                makeAndreiSprite(size), this, menu_selector(AndreiCreatorLayer::onAndreiLevels)
            );
            btn->setID("andrei-levels-button"_spr);
            menu->addChild(btn);
            menu->updateLayout();
            return true;
        }

        // fallback: buton sus, la mijloc
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto topMenu = CCMenu::create();
        topMenu->setPosition({winSize.width / 2.f, winSize.height - 22.f});
        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Andrei123459 Levels", 200, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .6f),
            this, menu_selector(AndreiCreatorLayer::onAndreiLevels)
        );
        btn->setID("andrei-levels-button"_spr);
        topMenu->addChild(btn);
        this->addChild(topMenu, 10);
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
