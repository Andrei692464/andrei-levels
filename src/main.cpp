#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>

using namespace geode::prelude;

static CCNode* makeAndreiTile(float S) {
    auto node = CCNode::create();
    node->setContentSize({S, S});

    auto bg = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
    auto bs = bg->getContentSize();
    bg->setScaleX(S / bs.width);
    bg->setScaleY(S / bs.height);
    bg->setPosition({S / 2.f, S / 2.f});
    node->addChild(bg);

    auto star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
    if (star) {
        star->setScale(S * 0.34f / star->getContentSize().height);
        star->setPosition({S / 2.f, S * 0.70f});
        node->addChild(star);
    }

    auto name = CCLabelBMFont::create("Andrei123459", "goldFont.fnt");
    name->limitLabelWidth(S * 0.9f, .5f, .1f);
    name->setPosition({S / 2.f, S * 0.36f});
    node->addChild(name);

    auto lvls = CCLabelBMFont::create("Levels", "goldFont.fnt");
    lvls->limitLabelWidth(S * 0.9f, .5f, .1f);
    lvls->setPosition({S / 2.f, S * 0.17f});
    node->addChild(lvls);

    return node;
}

class $modify(AndreiCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        const float S = 75.f;

        auto menu = CCMenu::create();
        menu->setID("andrei-levels-menu"_spr);
        menu->setPosition({S / 2.f + 6.f, winSize.height / 2.f});

        auto btn = CCMenuItemSpriteExtra::create(
            makeAndreiTile(S), this, menu_selector(AndreiCreatorLayer::onAndreiLevels)
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
