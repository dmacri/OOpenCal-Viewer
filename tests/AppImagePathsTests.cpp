#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include "core/AppImagePaths.h"

namespace
{
/** Sets environment variables for one test and restores them afterwards. */
class ScopedEnv
{
public:
    ScopedEnv(const char* name, const QByteArray& value) : m_name(name), m_old(qgetenv(name)), m_hadOld(qEnvironmentVariableIsSet(name))
    {
        if (value.isNull())
            qunsetenv(name);
        else
            qputenv(name, value);
    }

    ~ScopedEnv()
    {
        if (m_hadOld)
            qputenv(m_name, m_old);
        else
            qunsetenv(m_name);
    }

private:
    const char* m_name;
    QByteArray m_old;
    bool m_hadOld;
};

/** Changes the current directory for one test and restores it afterwards. */
class ScopedCwd
{
public:
    explicit ScopedCwd(const QString& dir) : m_old(QDir::currentPath()) { QDir::setCurrent(dir); }
    ~ScopedCwd() { QDir::setCurrent(m_old); }

private:
    QString m_old;
};
} // namespace

class AppImagePathsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_mount.isValid());
        ASSERT_TRUE(QDir(m_mount.path()).mkpath("compiler/gcc"));
        ASSERT_TRUE(QDir(m_other.path()).mkpath("models"));
    }

    QTemporaryDir m_mount; // plays the role of /tmp/.mount_XXXX
    QTemporaryDir m_other; // an ordinary directory elsewhere
};

TEST_F(AppImagePathsTest, NothingIsInsideTheImageWhenNotRunningFromAppImage)
{
    ScopedEnv appdir("APPDIR", QByteArray());
    ScopedEnv appimage("APPIMAGE", QByteArray());

    EXPECT_TRUE(viz::appimage::mountedRoot().isEmpty());
    EXPECT_FALSE(viz::appimage::isInsideMountedImage(m_mount.path()));
    EXPECT_FALSE(viz::appimage::isInsideMountedImage(m_mount.filePath("compiler/gcc")));
}

TEST_F(AppImagePathsTest, RootAndItsSubdirectoriesAreInsideTheImage)
{
    ScopedEnv appdir("APPDIR", m_mount.path().toUtf8());

    EXPECT_TRUE(viz::appimage::isInsideMountedImage(m_mount.path()));
    EXPECT_TRUE(viz::appimage::isInsideMountedImage(m_mount.filePath("compiler")));
    EXPECT_TRUE(viz::appimage::isInsideMountedImage(m_mount.filePath("compiler/gcc")));
}

TEST_F(AppImagePathsTest, OtherDirectoriesAreNotInsideTheImage)
{
    ScopedEnv appdir("APPDIR", m_mount.path().toUtf8());

    EXPECT_FALSE(viz::appimage::isInsideMountedImage(m_other.path()));
    EXPECT_FALSE(viz::appimage::isInsideMountedImage(QDir::homePath()));
    EXPECT_FALSE(viz::appimage::isInsideMountedImage(QString()));
}

TEST_F(AppImagePathsTest, SiblingWithTheSamePrefixIsNotInsideTheImage)
{
    // "/x/mount" must not match "/x/mount2": only whole path components count
    QDir parent(m_other.path());
    ASSERT_TRUE(parent.mkpath("mount"));
    ASSERT_TRUE(parent.mkpath("mount2"));
    ScopedEnv appdir("APPDIR", parent.filePath("mount").toUtf8());

    EXPECT_TRUE(viz::appimage::isInsideMountedImage(parent.filePath("mount")));
    EXPECT_FALSE(viz::appimage::isInsideMountedImage(parent.filePath("mount2")));
}

TEST_F(AppImagePathsTest, StartDirectoryIsTheLaunchDirectory)
{
    ScopedEnv appdir("APPDIR", m_mount.path().toUtf8());
    ScopedEnv owd("OWD", QByteArray());
    ScopedCwd cwd(m_other.path());

    EXPECT_EQ(QFileInfo(viz::appimage::defaultStartDirectory()).canonicalFilePath(), QFileInfo(m_other.path()).canonicalFilePath());
}

TEST_F(AppImagePathsTest, OriginalWorkingDirectoryOfTheRuntimeWins)
{
    ScopedEnv appdir("APPDIR", m_mount.path().toUtf8());
    ScopedEnv owd("OWD", m_other.filePath("models").toUtf8());
    ScopedCwd cwd(m_mount.path());

    EXPECT_EQ(QFileInfo(viz::appimage::defaultStartDirectory()).canonicalFilePath(), QFileInfo(m_other.filePath("models")).canonicalFilePath());
}

TEST_F(AppImagePathsTest, StartDirectoryNeverPointsInsideTheImage)
{
    // Launched from inside the mount (cwd = bundled toolchain) and nothing else known: the result must not be the mount
    ScopedEnv appdir("APPDIR", m_mount.path().toUtf8());
    ScopedEnv owd("OWD", QByteArray());
    ScopedEnv appimage("APPIMAGE", QByteArray());
    ScopedCwd cwd(m_mount.filePath("compiler/gcc"));

    const QString start = viz::appimage::defaultStartDirectory();
    EXPECT_FALSE(viz::appimage::isInsideMountedImage(start));
    EXPECT_EQ(start, QDir::homePath());
}

TEST_F(AppImagePathsTest, FallsBackToDirectoryOfTheAppImageFile)
{
    ScopedEnv appdir("APPDIR", m_mount.path().toUtf8());
    ScopedEnv owd("OWD", QByteArray());
    ScopedEnv appimage("APPIMAGE", m_other.filePath("OOpenCal-Viewer.AppImage").toUtf8());
    ScopedCwd cwd(m_mount.path());

    EXPECT_EQ(QFileInfo(viz::appimage::defaultStartDirectory()).canonicalFilePath(), QFileInfo(m_other.path()).canonicalFilePath());
}
