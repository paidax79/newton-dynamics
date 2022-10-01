package com.javaNewton;

import com.newton.ndWorldGlue;

import java.util.HashMap;

public class nWorld
{
    public nWorld()
    {
        m_bodyMap = new HashMap<>();
        m_nativeObject = new ndWorldGlue();
    }

    public void Sync()
    {
        m_nativeObject.Sync();
    }

    public void SetSubSteps(int substeps)
    {
        m_nativeObject.SetSubSteps(substeps);
    }

    public void Update(float timestep)
    {
        m_nativeObject.Update(timestep);
    }

    public void AddBody(nRigidBody body)
    {
        m_nativeObject.AddBody(body.GetNativeObject());
        m_bodyMap.put (body.GetId(), body);
    }

    public void RemoveBody(nRigidBody body)
    {
        m_nativeObject.RemoveBody(body.GetNativeObject());
        m_bodyMap.remove (body.GetId());
    }

    private ndWorldGlue m_nativeObject;
    private HashMap<Integer, nRigidBody> m_bodyMap = new HashMap<>();
}