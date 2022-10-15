#pragma once

#include <wrl/client.h>
#include <d3d11.h>

class CDXD3DDevice
{
public:
	CDXD3DDevice() : m_pDevice(nullptr), m_pDeviceContext(nullptr) { }
	CDXD3DDevice(const Microsoft::WRL::ComPtr<ID3D11Device> & pDevice, const Microsoft::WRL::ComPtr<ID3D11DeviceContext> & pDeviceContext) : m_pDevice(pDevice), m_pDeviceContext(pDeviceContext){}

	void SetDevice(const Microsoft::WRL::ComPtr<ID3D11Device> & d) { m_pDevice = d; }
	void setDeviceContext(const Microsoft::WRL::ComPtr<ID3D11DeviceContext> & d) { m_pDeviceContext = d; }

	Microsoft::WRL::ComPtr<ID3D11Device> GetDevice() { return m_pDevice; }
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> GetDeviceContext() { return m_pDeviceContext; }

protected:
	Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pDeviceContext;
};