pipeline {
    agent none
    parameters {
        string(name: 'BASE_VERSION', defaultValue: '1.0', description: 'From https://github.com/dernasherbrezon/sdr-modem/actions')
        string(name: 'BUILD_NUMBER', defaultValue: '77', description: 'From https://github.com/dernasherbrezon/sdr-modem/actions')
    }
    environment {
        GPG = credentials("GPG")
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
                            deleteDir()
                            git(url: 'git@github.com:dernasherbrezon/sdr-modem.git', branch: "${OS_CODENAME}", credentialsId: '5c8b3e93-0551-475c-9e54-1266242c8ff5', changelog: false)
                        }
                    }
                    stage('build and deploy') {
                        steps {
                            sh 'echo $GPG_PSW | /usr/lib/gnupg2/gpg-preset-passphrase -c $GPG_USR'
                            sh "bash ./build_and_deploy.sh ${CPU} ${OS_CODENAME} ${params.BASE_VERSION} ${params.BUILD_NUMBER} ${GPG_USR}"
                        }
                    }
                }
            }
        }
    }
}