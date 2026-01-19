import { useContext } from 'react';
import { Trans, useTranslation } from 'react-i18next';
import { FormCheck, FormLabel } from 'react-bootstrap';
import { NavLink } from 'react-router-dom';
import * as yup from 'yup';

import Section from '../Components/Section';
import { AppContext } from '../Contexts/AppContext';

import { AddonPropTypes } from '../Pages/AddonsConfigPage';

export const dualPicoHostScheme = {
	DualPicoHostAddonEnabled: yup
		.number()
		.required()
		.label('Dual Pico Host Add-On Enabled'),
};

export const dualPicoHostState = {
	DualPicoHostAddonEnabled: 0,
};

const DualPicoHost = ({
	values,
	errors,
	handleChange,
	handleCheckbox,
}: AddonPropTypes) => {
	const { t } = useTranslation();

	return (
		<Section
			title={t('AddonsConfig:dual-pico-host-header-text')}
		>
			<FormCheck
				label={t('Common:switch-enabled')}
				type="switch"
				id="DualPicoHostAddonButton"
				reverse
				isInvalid={false}
				checked={Boolean(values.DualPicoHostAddonEnabled)}
				onChange={(e) => {
					handleCheckbox('DualPicoHostAddonEnabled');
					handleChange(e);
				}}
			/>
		</Section>
	);
};

export default DualPicoHost;