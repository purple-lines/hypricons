#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <GLES3/gl32.h>

class CIconOverlay;

class CIconPassElement : public IPassElement {
  public:
    struct SIconData {
        CIconOverlay* overlay = nullptr;
    };

    CIconPassElement(const SIconData& data);
    virtual ~CIconPassElement() = default;

    virtual void                draw(const CRegion& damage) override;
    virtual bool                needsLiveBlur() override { return false; }
    virtual bool                needsPrecomputeBlur() override { return false; }
    virtual std::optional<CBox> boundingBox() override;
    virtual const char*         passName() override { return "CIconPassElement"; }

  private:
    SIconData m_data;
};
