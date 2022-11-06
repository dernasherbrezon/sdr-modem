pipeline {
    agent none
    parameters {
        string(name: 'VERSION', defaultValue: '1.0.77-77', description: 'From https://github.com/dernasherbrezon/sdr-modem/actions')
        string(name: 'UPSTREAM_VERSION', defaultValue: '1.0.77', description: 'From https://github.com/dernasherbrezon/sdr-modem/actions')
    }
    stages {
        stage('Package and deploy') {
            matrix {
                axes {
                    axis {
                        name 'OS_CODENAME'
                        values 'bullseye', 'buster', 'stretch'
                    }
                    axis {
                        name 'CPU'
                        values 'nocpuspecific'
                    }
                }
                agent {
                    label "${OS_CODENAME}"
                }
                stages {
                    stage('Checkout') {
                        steps {
                            sh 'echo "checking out ${OS_CODENAME}"'
                            checkout([
                              $class: 'GitSCM',
                              branches: [[name: "*/${OS_CODENAME}"]],
                              extensions: [[
                                $class: 'CloneOption',
                                shallow: true,
                                depth:   1,
                                timeout: 30
                              ]],
                              userRemoteConfigs: [[
                                url:           'git@github.com:dernasherbrezon/sdr-modem.git',
                                credentialsId: '5c8b3e93-0551-475c-9e54-1266242c8ff5'
                              ]]
                            ])
                            sh 'git config user.email "gpg@r2cloud.ru"'
                            sh 'git config user.name "r2cloud"'
                            sh 'git merge origin/main --no-edit'
                        }
                    }
                    stage('build debian package') {
                        steps {
                            sh 'GITHUB_ENV=env ./configure_flags.sh ${CPU} && ls -lh && . env'
                            sh 'gbp dch --auto --debian-branch=${OS_CODENAME} --upstream-branch=main --new-version=${params.VERSION}~${OS_CODENAME} --git-author --distribution=unstable --commit'
                            sh 'git push origin'
                            sh 'rm -f ../sdr-modem*deb'
                            sh 'gbp buildpackage --git-ignore-new --git-upstream-tag=${params.UPSTREAM_VERSION} --git-keyid=${GPG_KEYNAME}'
                        }
                    }
                    stage('deploy') {
                        steps {
                            sh 'cd .. && java -jar ${HOME}/${APT_CLI_VERSION}.jar --url s3://${BUCKET} --component main --codename ${OS_CODENAME} --gpg-keyname ${GPG_KEYNAME} --gpg-arguments "--pinentry-mode,loopback" save --patterns ./*.deb,./*.ddeb'
                        }
                    }
                }
            }
        }
    }
}