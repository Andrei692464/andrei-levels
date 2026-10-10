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
            if (size.width < 20.f || size.height < 20.f) size = {90.f, 90.f};

            auto btn = CCMenuItemSpriteExtra::create(
                makeAndreiSprite(size), this, menu_selector(AndreiCreatorLayer::onAndreiLevels)
            );
            btn->setID("andrei-levels-button"_spr);
            menu->addChild(btn);
            menu->updateLayout();
            return true;
        }

        // fallback: buton sus, la mijloc
