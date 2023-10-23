#pragma once

#include "D3DApp.h"

class BoxApp : public D3DApp {
public:

	BoxApp();
	~BoxApp();

	virtual bool Init(HINSTANCE hInstance, int nShowCmd) override;

private:

	virtual void Draw() override;

};
