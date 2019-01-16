#pragma once

#include <wrl/client.h>
#include <d3d11_3.h>

class CDXD3DDevice
{
public:
	CDXD3DDevice() : m_pDevice(nullptr), m_pDeviceContext(nullptr) { }
	CDXD3DDevice(const Microsoft::WRL::ComPtr<ID3D11Device3> & pDevice, const Microsoft::WRL::ComPtr<ID3D11DeviceContext4> & pDeviceContext) : m_pDevice(pDevice), m_pDeviceContext(pDeviceContext){}

	void SetDevice(const Microsoft::WRL::ComPtr<ID3D11Device3> & d) { m_pDevice = d; }
	void setDeviceContext(const Microsoft::WRL::ComPtr<ID3D11DeviceContext4> & d) { m_pDeviceContext = d; }

	Microsoft::WRL::ComPtr<ID3D11Device3> GetDevice() { return m_pDevice; }
	Microsoft::WRL::ComPtr<ID3D11DeviceContext4> GetDeviceContext() { return m_pDeviceContext; }

protected:
	Microsoft::WRL::ComPtr<ID3D11Device3> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_pDeviceContext;
};