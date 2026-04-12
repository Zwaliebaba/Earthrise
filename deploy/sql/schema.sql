-- EarthRise Database Schema
-- Target: Microsoft SQL Server 2022
-- Run via: sqlcmd -S localhost -U sa -P <password> -i schema.sql

-- Create the database if it doesn't exist.
IF DB_ID('Earthrise') IS NULL
    CREATE DATABASE Earthrise;
GO

USE Earthrise;
GO

-- ============================================================================
-- Accounts — player authentication (username/password, hashed).
-- ============================================================================
IF OBJECT_ID('dbo.Accounts', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.Accounts
    (
        Id            BIGINT         IDENTITY(1,1) PRIMARY KEY,
        Username      NVARCHAR(64)   NOT NULL,
        PasswordHash  VARCHAR(128)   NOT NULL,   -- hex-encoded PBKDF2-SHA256
        Salt          VARCHAR(64)    NOT NULL,   -- hex-encoded random salt
        CreatedAt     DATETIME2      NOT NULL DEFAULT SYSUTCDATETIME(),
        LastLogin     DATETIME2      NULL,

        CONSTRAINT UQ_Accounts_Username UNIQUE (Username)
    );

    CREATE NONCLUSTERED INDEX IX_Accounts_Username
        ON dbo.Accounts (Username);
END;
GO

-- ============================================================================
-- Ships — player-owned ships (fleet).
-- ============================================================================
IF OBJECT_ID('dbo.Ships', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.Ships
    (
        Id            BIGINT         IDENTITY(1,1) PRIMARY KEY,
        AccountId     BIGINT         NOT NULL,
        ShipDefIndex  INT            NOT NULL,   -- index into ObjectDefs ship table
        ShipName      NVARCHAR(64)   NULL,
        IsFlagship    BIT            NOT NULL DEFAULT 0,

        CONSTRAINT FK_Ships_Account
            FOREIGN KEY (AccountId) REFERENCES dbo.Accounts(Id)
            ON DELETE CASCADE
    );

    CREATE NONCLUSTERED INDEX IX_Ships_AccountId
        ON dbo.Ships (AccountId);
END;
GO

-- ============================================================================
-- Inventory — items owned by a player account.
-- ============================================================================
IF OBJECT_ID('dbo.Inventory', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.Inventory
    (
        Id            BIGINT         IDENTITY(1,1) PRIMARY KEY,
        AccountId     BIGINT         NOT NULL,
        ItemType      INT            NOT NULL,
        Quantity       INT            NOT NULL DEFAULT 1,
        Metadata      NVARCHAR(MAX)  NULL,       -- JSON for extensible item data

        CONSTRAINT FK_Inventory_Account
            FOREIGN KEY (AccountId) REFERENCES dbo.Accounts(Id)
            ON DELETE CASCADE
    );

    CREATE NONCLUSTERED INDEX IX_Inventory_AccountId
        ON dbo.Inventory (AccountId);
END;
GO

-- ============================================================================
-- WorldState — persistent object positions (for zone state save/restore).
-- ============================================================================
IF OBJECT_ID('dbo.WorldState', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.WorldState
    (
        Id            BIGINT         IDENTITY(1,1) PRIMARY KEY,
        ZoneId        INT            NOT NULL,
        Category      INT            NOT NULL,   -- SpaceObjectCategory enum value
        DefIndex      INT            NOT NULL,
        PosX          FLOAT          NOT NULL,
        PosY          FLOAT          NOT NULL,
        PosZ          FLOAT          NOT NULL,
        RotX          FLOAT          NOT NULL DEFAULT 0,
        RotY          FLOAT          NOT NULL DEFAULT 0,
        RotZ          FLOAT          NOT NULL DEFAULT 0,
        RotW          FLOAT          NOT NULL DEFAULT 1,
        SavedAt       DATETIME2      NOT NULL DEFAULT SYSUTCDATETIME()
    );

    CREATE NONCLUSTERED INDEX IX_WorldState_ZoneId
        ON dbo.WorldState (ZoneId);
END;
GO
