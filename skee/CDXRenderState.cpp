#include "CDXRenderState.h"

void CDXRenderState::BackupRenderState(CDXD3DDevice * device)
{
	auto ctx = device->GetDeviceContext();

	ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_backupState.RenderTargetViews, &m_backupState.DepthStencilView);

	m_backupState.ScissorRectsCount = m_backupState.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	ctx->RSGetScissorRects(&m_backupState.ScissorRectsCount, m_backupState.ScissorRects);
	ctx->RSGetViewports(&m_backupState.ViewportsCount, m_backupState.Viewports);
	ctx->RSGetState(&m_backupState.RS);
	ctx->OMGetBlendState(&m_backupState.BlendState, m_backupState.BlendFactor, &m_backupState.SampleMask);
	ctx->OMGetDepthStencilState(&m_backupState.DepthStencilState, &m_backupState.StencilRef);
	ctx->PSGetShaderResources(0, 1, &m_backupState.PSShaderResource);
	ctx->PSGetSamplers(0, 1, &m_backupState.PSSampler);
	ctx->GSGetShaderResources(0, 1, &m_backupState.GSShaderResource);
	m_backupState.PSInstancesCount = m_backupState.VSInstancesCount = m_backupState.GSInstancesCount = 256;
	ctx->PSGetShader(&m_backupState.PS, m_backupState.PSInstances, &m_backupState.PSInstancesCount);
	ctx->VSGetShader(&m_backupState.VS, m_backupState.VSInstances, &m_backupState.VSInstancesCount);
	ctx->GSGetShader(&m_backupState.GS, m_backupState.GSInstances, &m_backupState.GSInstancesCount);
	ctx->GSGetConstantBuffers(0, 1, &m_backupState.GSConstantBuffer);
	ctx->VSGetConstantBuffers(0, 1, &m_backupState.VSConstantBuffer);
	ctx->IAGetPrimitiveTopology(&m_backupState.PrimitiveTopology);
	ctx->IAGetIndexBuffer(&m_backupState.IndexBuffer, &m_backupState.IndexBufferFormat, &m_backupState.IndexBufferOffset);
	ctx->IAGetVertexBuffers(0, 1, &m_backupState.VertexBuffer, &m_backupState.VertexBufferStride, &m_backupState.VertexBufferOffset);
	ctx->IAGetInputLayout(&m_backupState.InputLayout);
}

void CDXRenderState::RestoreRenderState(CDXD3DDevice * device)
{
	auto ctx = device->GetDeviceContext();

	ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_backupState.RenderTargetViews, m_backupState.DepthStencilView);
	for (UInt32 i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
		if (m_backupState.RenderTargetViews[i]) {
			m_backupState.RenderTargetViews[i]->Release();
		}
	}
	if (m_backupState.DepthStencilView) m_backupState.DepthStencilView->Release();

	// Restore modified DX state
	ctx->RSSetScissorRects(m_backupState.ScissorRectsCount, m_backupState.ScissorRects);
	ctx->RSSetViewports(m_backupState.ViewportsCount, m_backupState.Viewports);
	ctx->RSSetState(m_backupState.RS); if (m_backupState.RS) m_backupState.RS->Release();
	ctx->OMSetBlendState(m_backupState.BlendState, m_backupState.BlendFactor, m_backupState.SampleMask); if (m_backupState.BlendState) m_backupState.BlendState->Release();
	ctx->OMSetDepthStencilState(m_backupState.DepthStencilState, m_backupState.StencilRef); if (m_backupState.DepthStencilState) m_backupState.DepthStencilState->Release();
	ctx->PSSetShaderResources(0, 1, &m_backupState.PSShaderResource); if (m_backupState.PSShaderResource) m_backupState.PSShaderResource->Release();
	ctx->PSSetSamplers(0, 1, &m_backupState.PSSampler); if (m_backupState.PSSampler) m_backupState.PSSampler->Release();
	ctx->PSSetShader(m_backupState.PS, m_backupState.PSInstances, m_backupState.PSInstancesCount); if (m_backupState.PS) m_backupState.PS->Release();
	for (UINT i = 0; i < m_backupState.PSInstancesCount; i++) if (m_backupState.PSInstances[i]) m_backupState.PSInstances[i]->Release();
	ctx->VSSetShader(m_backupState.VS, m_backupState.VSInstances, m_backupState.VSInstancesCount); if (m_backupState.VS) m_backupState.VS->Release();
	ctx->VSSetConstantBuffers(0, 1, &m_backupState.VSConstantBuffer); if (m_backupState.VSConstantBuffer) m_backupState.VSConstantBuffer->Release();
	for (UINT i = 0; i < m_backupState.VSInstancesCount; i++) if (m_backupState.VSInstances[i]) m_backupState.VSInstances[i]->Release();
	ctx->GSSetShader(m_backupState.GS, m_backupState.GSInstances, m_backupState.GSInstancesCount); if (m_backupState.GS) m_backupState.GS->Release();
	ctx->GSSetShaderResources(0, 1, &m_backupState.GSShaderResource); if (m_backupState.GSShaderResource) m_backupState.GSShaderResource->Release();
	ctx->GSSetConstantBuffers(0, 1, &m_backupState.GSConstantBuffer); if (m_backupState.GSConstantBuffer) m_backupState.GSConstantBuffer->Release();
	for (UINT i = 0; i < m_backupState.GSInstancesCount; i++) if (m_backupState.GSInstances[i]) m_backupState.GSInstances[i]->Release();
	ctx->IASetPrimitiveTopology(m_backupState.PrimitiveTopology);
	ctx->IASetIndexBuffer(m_backupState.IndexBuffer, m_backupState.IndexBufferFormat, m_backupState.IndexBufferOffset); if (m_backupState.IndexBuffer) m_backupState.IndexBuffer->Release();
	ctx->IASetVertexBuffers(0, 1, &m_backupState.VertexBuffer, &m_backupState.VertexBufferStride, &m_backupState.VertexBufferOffset); if (m_backupState.VertexBuffer) m_backupState.VertexBuffer->Release();
	ctx->IASetInputLayout(m_backupState.InputLayout); if (m_backupState.InputLayout) m_backupState.InputLayout->Release();
}